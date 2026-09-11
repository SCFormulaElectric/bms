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
    ams_afe_status_t afe_status;
    uint32_t sample_counter = 0U;
    uint32_t last_telemetry_ms = 0U;

    ams_battery_initialize(&battery, config, data->initial_soc_permille);
    ams_can_cursor_reset(&can_cursor);
    ams_afe_reset(&afe);
    ams_controller_initialize(&controller, config);
    afe_status = ams_afe_initialize(&afe, &hspi1,
        ams_afe_default_profile());

    for (;;) {
        ams_measurement_t measurement;
        uint32_t immediate_faults = AMS_FAULT_NONE;
        uint8_t decision_ready = 0U;
        const uint32_t now_ms = (uint32_t)HAL_GetTick();

        if (data->afe_bringup_status != (uint8_t)BQ79600_BRINGUP_OK) {
            immediate_faults |= AMS_FAULT_AFE_COMMUNICATION;
        }

        afe_status = ams_afe_service(&afe, now_ms);
        if (afe_status == AMS_AFE_NEW_SAMPLE) {
            if (ams_afe_fetch_latest(&afe, &measurement,
                &sample_counter) == AMS_AFE_OK) {
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
                decision_ready = 1U;
            } else {
                afe_status = AMS_AFE_INVALID_DATA;
            }
        }
        if (afe_status != AMS_AFE_OK &&
            afe_status != AMS_AFE_IN_PROGRESS &&
            afe_status != AMS_AFE_NEW_SAMPLE) {
            memset(&measurement, 0, sizeof(measurement));
            measurement.timestamp_ms = now_ms;
            measurement.status = AMS_SAMPLE_UNAVAILABLE;
            immediate_faults |= AMS_FAULT_AFE_COMMUNICATION;
            decision_ready = 1U;
        }

        if (decision_ready != 0U) {
            if (measurement.status == AMS_SAMPLE_VALID) {
                ams_battery_update(&battery, config, &measurement, now_ms);
            }
            ams_controller_step(&controller, &measurement, immediate_faults,
                now_ms);
            apply_safety_outputs(&controller.decision);
            HAL_GPIO_WritePin(AMS_STATUS_LED_GPIO_PORT, AMS_STATUS_LED_PIN,
                (controller.decision.latched_faults == AMS_FAULT_NONE) ?
                    GPIO_PIN_SET : GPIO_PIN_RESET);

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
            }
            data->first_fault = controller.decision.first_fault;
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
            xEventGroupSetBits(data->watchdog_events, WD_AMS_CYCLE);
        }

        vTaskDelay((afe_status == AMS_AFE_IN_PROGRESS) ?
            pdMS_TO_TICKS(AMS_AFE_SERVICE_POLL_MS) :
            pdMS_TO_TICKS(AMS_CRITICAL_CYCLE_PERIOD_MS));
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
