#include "Tasks/CAN/bms_summary_task.h"

#include <string.h>

#include "Peripherals/bms_can_protocol.h"
#include "Peripherals/bms_summary_config.h"

static void capture_snapshot(const app_data_t *data,
    bms_summary_values_t *values, bms_summary_config_t *config)
{
    taskENTER_CRITICAL();
    values->state = (uint8_t)data->ams_state;
    values->soc_permille = data->soc_permille;
    values->pack_voltage_mv = data->pack_voltage_mv;
    values->pack_current_ma = data->pack_current_ma;
    values->minimum_cell_mv = data->minimum_cell_mv;
    values->maximum_cell_mv = data->maximum_cell_mv;
    values->maximum_temperature_dc = data->maximum_temperature_dc;
    values->active_faults = data->active_faults;
    values->latched_faults = data->latched_faults;
    values->discharge_enable_request = data->discharge_enable_request;
    values->charge_enable_request = data->charge_enable_request;
    values->fan_enable_request = data->fan_enable_request;
    values->valid_cell_count = data->valid_cell_count;
    values->valid_temperature_count = data->valid_temperature_count;
    memcpy(config, &data->summary_config, sizeof(*config));
    taskEXIT_CRITICAL();
}

void bms_summary_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        bms_summary_values_t values = {0};
        bms_summary_config_t config;
        uint32_t slot_index;

        capture_snapshot(data, &values, &config);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(config.period_ms));
        capture_snapshot(data, &values, &config);
        if (bms_summary_config_is_valid(&config) == 0U) {
            continue;
        }
        for (slot_index = 0U; slot_index < config.active_count;
            slot_index++) {
            can_tx_message_t message = {0};

            message.id = BMS_CAN_SUMMARY_BASE_ID + slot_index;
            message.dlc = BMS_CAN_FRAME_DLC;
            if (bms_summary_pack_frame(&config, (uint8_t)slot_index,
                &values, message.data) != 0U) {
                (void)can_bus_queue_message(&data->can_bus, &message);
            }
        }
    }
}

task_entry_t create_bms_summary_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(bms_summary_task,
        "BMS 5s summary", BMS_SUMMARY_STACK_SIZE, data,
        BMS_SUMMARY_PRIO, &entry.handle);

    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);
    entry.name = "bms_summary";
    return entry;
}
