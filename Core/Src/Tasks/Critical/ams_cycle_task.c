#include "Tasks/Critical/ams_cycle_task.h"

#include <string.h>

#include "Ams/ams_afe.h"
#include "Ams/ams_battery.h"
#include "Ams/ams_controller.h"
#include "Ams/bq79600_bridge.h"
#include "main.h"
#include "Peripherals/adc.h"
#include "Peripherals/ams_can_telemetry.h"
#include "Peripherals/can_protocol.h"
#include "Peripherals/digital_pins.h"
#include "Tasks/Task_Helper/watchdog_tasks_defs.h"

static void apply_safety_outputs(const ams_decision_t *decision)
{
    GPIO_PinState discharge = AMS_OUTPUT_DEASSERTED;
    GPIO_PinState charge = AMS_OUTPUT_DEASSERTED;
    GPIO_PinState fan = AMS_OUTPUT_DEASSERTED;

    if (decision != NULL && AMS_HARDWARE_OUTPUTS_COMMISSIONED != 0U) {
        if (decision->discharge_enable_request != 0U &&
            decision->latched_faults == AMS_FAULT_NONE) {
            discharge = AMS_OUTPUT_ASSERTED;
        }
        if (decision->charge_enable_request != 0U &&
            decision->latched_faults == AMS_FAULT_NONE) {
            charge = AMS_OUTPUT_ASSERTED;
        }
        if (decision->fan_enable_request != 0U) {
            fan = AMS_OUTPUT_ASSERTED;
        }
    }
    HAL_GPIO_WritePin(AMS_DISCHARGE_ENABLE_GPIO_PORT,
        AMS_DISCHARGE_ENABLE_PIN, discharge);
    HAL_GPIO_WritePin(AMS_CHARGE_ENABLE_GPIO_PORT,
        AMS_CHARGE_ENABLE_PIN, charge);
    HAL_GPIO_WritePin(AMS_FAN_ENABLE_GPIO_PORT, AMS_FAN_ENABLE_PIN, fan);
}

static uint8_t service_fault_clear_request(app_data_t *data,
    ams_controller_t *controller)
{
    uint8_t requested;

    taskENTER_CRITICAL();
    requested = data->fault_clear_requested;
    data->fault_clear_requested = 0U;
    taskEXIT_CRITICAL();
    if (requested == 0U) {
        return 0U;
    }

    ams_controller_clear_faults(controller);
    apply_safety_outputs(&controller->decision);
    taskENTER_CRITICAL();
    data->ams_state = controller->decision.state;
    data->active_faults = controller->decision.active_faults;
    data->latched_faults = controller->decision.latched_faults;
    data->discharge_enable_request = 0U;
    data->charge_enable_request = 0U;
    data->fan_enable_request = 0U;
    data->first_fault = controller->decision.first_fault;
    taskEXIT_CRITICAL();
    return 1U;
}

void ams_afe_bring_up(app_data_t *data)
{
    bq79600_bringup_result_t bringup;
    bq79600_comm_diag_t diag;
    const uint8_t scheduler_running =
        (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? 1U : 0U;

    memset(&diag, 0, sizeof(diag));
    (void)bq79600_stack_bringup(&hspi1, data->config.segment_count,
        &bringup);
    if (bringup.status == BQ79600_BRINGUP_OK) {
        /* GPIO thermistor inputs are read as voltages, not temperatures. */
        (void)bq79600_configure_measurements(&hspi1,
            data->config.segment_count, data->config.cells_per_segment,
            data->config.temperatures_per_segment, &bringup);
    }
#if AMS_BALANCE_BENCH_TEST_CELL != 0U
    if (bringup.status == BQ79600_BRINGUP_OK &&
        AMS_BALANCE_BENCH_TEST_CELL <= data->config.cells_per_segment) {
        /* Bench-only timed bleed of one cell; see ams_config.h. */
        (void)bq79600_start_single_cell_balance(&hspi1, 1U,
            AMS_BALANCE_BENCH_TEST_CELL, AMS_BALANCE_BENCH_TEST_TIMER,
            &bringup);
    }
#endif
    if (bringup.status != BQ79600_BRINGUP_OK &&
        bringup.failed_step >= BQ79600_STEP_STACK_WAKE) {
        /* The bridge is awake, so record why the stack did not answer. */
        (void)bq79600_read_comm_diagnostics(&hspi1, &diag);
        if (bringup.bridge_device_config == 0U &&
            diag.read_status == BQ79600_BRINGUP_OK) {
            bringup.bridge_device_config = diag.device_config;
        }
    }

    /* Before the scheduler starts a critical section would leave interrupts
     * masked until vTaskStartScheduler, and no other task can read yet. */
    if (scheduler_running != 0U) {
        taskENTER_CRITICAL();
    }
    data->afe_bringup_status = (uint8_t)bringup.status;
    data->afe_bringup_failed_step = (uint8_t)bringup.failed_step;
    data->afe_verified_devices = bringup.verified_stack_devices;
    data->afe_bridge_device_config = bringup.bridge_device_config;
    data->afe_comm_diag = diag;
    data->afe_bringup_attempts++;
    if (scheduler_running != 0U) {
        taskEXIT_CRITICAL();
    }
}

static int32_t sum_pack_voltage(const ams_measurement_t *measurement)
{
    uint16_t index;
    int32_t total = 0;

    for (index = 0U; index < measurement->valid_cell_count; index++) {
        total += measurement->cell_voltage_mv[index];
    }
    return total;
}

void ams_cycle_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    static ams_afe_t afe;
    static ams_controller_t controller;
    static ams_battery_state_t battery;
    static ams_can_cursor_t can_cursor;
    const ams_config_t *config = &data->config;
    uint32_t last_telemetry_ms = 0U;
#ifdef DEBUG
    uint32_t last_bringup_ms = (uint32_t)HAL_GetTick();
#endif
#if AMS_BALANCE_BENCH_TEST_CELL != 0U
    uint32_t last_balance_ms = 0U;
#endif
    ams_afe_status_t afe_status;

    ams_battery_initialize(&battery, config, data->initial_soc_permille);
    ams_can_cursor_reset(&can_cursor);
    ams_controller_initialize(&controller, config);
    (void)ams_afe_initialize(&afe, &hspi1, config->segment_count,
        config->cells_per_segment, config->temperatures_per_segment);

    for (;;) {
        ams_measurement_t measurement;
        uint32_t immediate_faults = AMS_FAULT_NONE;
        uint8_t decision_ready = 0U;
        const uint32_t now_ms = (uint32_t)HAL_GetTick();

        if (service_fault_clear_request(data, &controller) != 0U) {
            xEventGroupSetBits(data->watchdog_events, WD_AMS_CYCLE);
            vTaskDelay(pdMS_TO_TICKS(AMS_CRITICAL_CYCLE_PERIOD_MS));
            continue;
        }

#ifdef DEBUG
        /* Bench aid: retry a failed bring-up so the stack can be powered or
         * rewired without resetting the MCU. The AFE reader has never run,
         * so the SPI is idle. Each attempt busy-waits for tens of ms and
         * masks interrupts during the two 2.75 ms wake pings. */
        if (data->afe_bringup_status != (uint8_t)BQ79600_BRINGUP_OK &&
            (uint32_t)(now_ms - last_bringup_ms) >=
                AMS_AFE_BRINGUP_RETRY_MS) {
            ams_afe_bring_up(data);
            last_bringup_ms = (uint32_t)HAL_GetTick();
            /* now_ms is stale after the busy-wait; start a fresh cycle. */
            xEventGroupSetBits(data->watchdog_events, WD_AMS_CYCLE);
            vTaskDelay(pdMS_TO_TICKS(AMS_CRITICAL_CYCLE_PERIOD_MS));
            continue;
        }
#endif

        /* The BQ79616s convert continuously (configured at boot). The AFE
         * pipeline reads the latest results over DMA without blocking; poll
         * it every millisecond until a sample completes or fails. */
        afe_status = (data->afe_bringup_status ==
            (uint8_t)BQ79600_BRINGUP_OK) ?
            ams_afe_service(&afe, now_ms, &measurement) :
            AMS_AFE_NOT_COMMISSIONED;
        if (afe_status == AMS_AFE_NEW_SAMPLE) {
            adc_snapshot_t current_snapshot;
            if (adc_acquisition_read(&current_snapshot, now_ms) ==
                ADC_SNAPSHOT_OK) {
                measurement.pack_current_ma =
                    ams_config_convert_current_ma(config,
                        current_snapshot.channels[
                            config->current_adc_channel]);
            } else {
                immediate_faults |= AMS_FAULT_CURRENT_SENSOR;
            }
            measurement.pack_voltage_mv = sum_pack_voltage(&measurement);
            measurement.timestamp_ms = now_ms;
            decision_ready = 1U;
        } else if (afe_status != AMS_AFE_IN_PROGRESS) {
            memset(&measurement, 0, sizeof(measurement));
            measurement.timestamp_ms = now_ms;
            measurement.status = AMS_SAMPLE_UNAVAILABLE;
            immediate_faults |= AMS_FAULT_AFE_COMMUNICATION;
            decision_ready = 1U;
        }
        if (decision_ready == 0U) {
            vTaskDelay(pdMS_TO_TICKS(AMS_AFE_SERVICE_POLL_MS));
            continue;
        }

        if (measurement.status == AMS_SAMPLE_VALID) {
            ams_battery_update(&battery, config, &measurement, now_ms);
        }
        ams_controller_step(&controller, &measurement, immediate_faults,
            now_ms);
        apply_safety_outputs(&controller.decision);

        taskENTER_CRITICAL();
        data->ams_state = controller.decision.state;
        data->active_faults = controller.decision.active_faults;
        data->latched_faults = controller.decision.latched_faults;
        data->discharge_enable_request =
            controller.decision.discharge_enable_request;
        data->charge_enable_request =
            controller.decision.charge_enable_request;
        data->fan_enable_request =
            controller.decision.fan_enable_request;
        data->soc_permille = battery.soc_permille;
        if (measurement.status == AMS_SAMPLE_VALID) {
            data->pack_voltage_mv = measurement.pack_voltage_mv;
            data->pack_current_ma = measurement.pack_current_ma;
            data->minimum_cell_mv = battery.minimum_cell_mv;
            data->maximum_cell_mv = battery.maximum_cell_mv;
            data->maximum_temperature_dc =
                battery.maximum_temperature_dc;
            data->valid_cell_count = measurement.valid_cell_count;
            data->valid_temperature_count =
                measurement.valid_temperature_count;
            data->last_measurement = measurement;
        }
        data->first_fault = controller.decision.first_fault;
        data->afe_sample_counter = afe.sample_counter;
        data->afe_communication_errors = afe.communication_errors;
        data->afe_last_error = (uint8_t)afe.last_error;
        taskEXIT_CRITICAL();

        if (CAN_BMS_FAST_TELEMETRY_ENABLED &&
            (uint32_t)(now_ms - last_telemetry_ms) >=
                CAN_AMS_TELEMETRY_PERIOD_MS) {
            ams_can_publish_snapshot(&data->can_bus, &can_cursor, config,
                &measurement, &battery, &controller.decision,
                data->afe_bringup_status,
                data->afe_bringup_failed_step,
                data->afe_verified_devices,
                data->afe_bridge_device_config);
            last_telemetry_ms = now_ms;
        }
#if AMS_BALANCE_BENCH_TEST_CELL != 0U
        /* Bench balance test: right after a sample the AFE reader is idle
         * until the next ams_afe_service call, so a short blocking read of
         * the balance status cannot collide with its DMA transfers. */
        if (afe_status == AMS_AFE_NEW_SAMPLE &&
            (uint32_t)(now_ms - last_balance_ms) >=
                AMS_BALANCE_STATUS_PERIOD_MS) {
            bq79616_balance_status_t balance = {0};
            const bq79600_bringup_status_t read =
                bq79600_read_balance_status(&hspi1, 1U,
                    AMS_BALANCE_BENCH_TEST_CELL, &balance);

            last_balance_ms = now_ms;
            taskENTER_CRITICAL();
            data->balance_status = balance;
            data->balance_read_status = (uint8_t)read;
            data->balance_reads++;
            taskEXIT_CRITICAL();
        }
#endif
        xEventGroupSetBits(data->watchdog_events, WD_AMS_CYCLE);

        vTaskDelay(pdMS_TO_TICKS(AMS_CRITICAL_CYCLE_PERIOD_MS));
    }
}

task_entry_t create_ams_cycle_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(ams_cycle_task, "AMS critical cycle",
        AMS_CYCLE_STACK_SIZE, data, AMS_CYCLE_PRIO, &entry.handle);
    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);
    entry.name = "ams_cycle";
    return entry;
}
