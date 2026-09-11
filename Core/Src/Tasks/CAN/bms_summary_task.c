#include "Tasks/CAN/bms_summary_task.h"

#include <limits.h>

#include "Peripherals/can_protocol.h"

typedef struct {
    ams_state_t state;
    uint16_t soc_permille;
    int32_t pack_voltage_mv;
    int32_t pack_current_ma;
    uint16_t minimum_cell_mv;
    uint16_t maximum_cell_mv;
    int16_t maximum_temperature_dc;
    uint32_t active_faults;
} bms_summary_snapshot_t;

static void put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static int16_t clamp_i16(int32_t value)
{
    if (value > INT16_MAX) {
        return INT16_MAX;
    }
    if (value < INT16_MIN) {
        return INT16_MIN;
    }
    return (int16_t)value;
}

static uint16_t clamp_u16(int32_t value)
{
    if (value <= 0) {
        return 0U;
    }
    if (value > UINT16_MAX) {
        return UINT16_MAX;
    }
    return (uint16_t)value;
}

static void capture_snapshot(const app_data_t *data,
    bms_summary_snapshot_t *snapshot)
{
    taskENTER_CRITICAL();
    snapshot->state = data->ams_state;
    snapshot->soc_permille = data->soc_permille;
    snapshot->pack_voltage_mv = data->pack_voltage_mv;
    snapshot->pack_current_ma = data->pack_current_ma;
    snapshot->minimum_cell_mv = data->minimum_cell_mv;
    snapshot->maximum_cell_mv = data->maximum_cell_mv;
    snapshot->maximum_temperature_dc = data->maximum_temperature_dc;
    snapshot->active_faults = data->active_faults;
    taskEXIT_CRITICAL();
}

static void queue_primary(can_bus_t *bus, uint16_t id,
    const bms_summary_snapshot_t *snapshot)
{
    can_tx_message_t message = {0};

    message.id = id;
    message.dlc = 8U;
    message.data[0] = BMS_SUMMARY_FORMAT_VERSION;
    message.data[1] = (uint8_t)snapshot->state;
    put_u16(&message.data[2], snapshot->soc_permille);
    put_u16(&message.data[4], clamp_u16(snapshot->pack_voltage_mv / 100));
    put_u16(&message.data[6],
        (uint16_t)clamp_i16(snapshot->pack_current_ma / 100));
    (void)can_bus_queue_message(bus, &message);
}

static void queue_secondary(can_bus_t *bus, uint16_t id,
    const bms_summary_snapshot_t *snapshot)
{
    can_tx_message_t message = {0};

    message.id = id;
    message.dlc = 8U;
    put_u16(&message.data[0], snapshot->minimum_cell_mv);
    put_u16(&message.data[2], snapshot->maximum_cell_mv);
    put_u16(&message.data[4],
        (uint16_t)snapshot->maximum_temperature_dc);
    put_u16(&message.data[6], (uint16_t)snapshot->active_faults);
    (void)can_bus_queue_message(bus, &message);
}

void bms_summary_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        bms_summary_snapshot_t snapshot;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BMS_SUMMARY_PERIOD_MS));
        capture_snapshot(data, &snapshot);
        if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_SUMMARY_PRIMARY)) {
            queue_primary(&data->can_bus, CAN_ID_AMS_SUMMARY_PRIMARY,
                &snapshot);
        }
        if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_SUMMARY_SECONDARY)) {
            queue_secondary(&data->can_bus, CAN_ID_AMS_SUMMARY_SECONDARY,
                &snapshot);
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
