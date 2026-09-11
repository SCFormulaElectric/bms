#include "Peripherals/ams_can_telemetry.h"

#include <stddef.h>
#include <string.h>

#include "Peripherals/can_protocol.h"

static void put_u16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
    data[2] = (uint8_t)(value >> 16);
    data[3] = (uint8_t)(value >> 24);
}

static void send_frame(can_bus_t *bus, uint16_t id, const uint8_t *payload)
{
    can_tx_message_t message = {0};

    message.id = id;
    message.dlc = 8U;
    memcpy(message.data, payload, sizeof(message.data));
    (void)can_bus_queue_message(bus, &message);
}

void ams_can_cursor_reset(ams_can_cursor_t *cursor)
{
    if (cursor != NULL) {
        memset(cursor, 0, sizeof(*cursor));
    }
}

void ams_can_publish_snapshot(can_bus_t *bus, ams_can_cursor_t *cursor,
    const ams_config_t *config, const ams_measurement_t *measurement,
    const ams_battery_state_t *battery, const ams_decision_t *decision,
    uint8_t afe_bringup_status, uint8_t afe_bringup_failed_step,
    uint8_t afe_verified_devices, uint8_t afe_bridge_device_config)
{
    uint8_t payload[8] = {0};
    uint16_t count;
    uint16_t index;

    if (bus == NULL || cursor == NULL || config == NULL ||
        measurement == NULL || battery == NULL || decision == NULL) {
        return;
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_HEARTBEAT)) {
        payload[0] = (uint8_t)decision->state;
        payload[1] = cursor->rolling_counter++;
        put_u16(&payload[2], battery->soc_permille);
        put_u32(&payload[4], measurement->timestamp_ms);
        send_frame(bus, CAN_ID_AMS_HEARTBEAT, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_PACK_STATUS)) {
        put_u32(&payload[0], (uint32_t)measurement->pack_voltage_mv);
        put_u32(&payload[4], (uint32_t)measurement->pack_current_ma);
        send_frame(bus, CAN_ID_AMS_PACK_STATUS, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_CELL_EXTREMA)) {
        put_u16(&payload[0], battery->minimum_cell_mv);
        put_u16(&payload[2], battery->maximum_cell_mv);
        payload[4] = (uint8_t)battery->minimum_cell_index;
        payload[5] = (uint8_t)battery->maximum_cell_index;
        put_u16(&payload[6], battery->soc_permille);
        send_frame(bus, CAN_ID_AMS_CELL_EXTREMA, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_TEMPERATURES)) {
        put_u16(&payload[0], (uint16_t)battery->minimum_temperature_dc);
        put_u16(&payload[2], (uint16_t)battery->maximum_temperature_dc);
        put_u16(&payload[4], measurement->valid_temperature_count);
        put_u16(&payload[6], measurement->valid_cell_count);
        send_frame(bus, CAN_ID_AMS_TEMPERATURES, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_FAULTS)) {
        put_u32(&payload[0], decision->active_faults);
        put_u32(&payload[4], decision->latched_faults);
        send_frame(bus, CAN_ID_AMS_FAULTS, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_OUTPUTS_LIMITS)) {
        memset(payload, 0, sizeof(payload));
        payload[0] = (uint8_t)(decision->discharge_enable_request |
            (decision->charge_enable_request << 1) |
            (decision->fan_enable_request << 2) |
            (battery->empty_anchor_active << 3) |
            (battery->full_anchor_active << 4));
        payload[1] = (uint8_t)config->current_sensor_polarity;
        put_u16(&payload[2],
            (uint16_t)(config->discharge_current_limit_ma / 1000U));
        put_u16(&payload[4],
            (uint16_t)(config->charge_current_limit_ma / 1000U));
        payload[6] = config->segment_count;
        payload[7] = config->cells_per_segment;
        send_frame(bus, CAN_ID_AMS_OUTPUTS_LIMITS, payload);
    }

    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_AFE_STATUS)) {
        memset(payload, 0, sizeof(payload));
        payload[0] = afe_bringup_status;
        payload[1] = afe_bringup_failed_step;
        payload[2] = config->segment_count;
        payload[3] = afe_verified_devices;
        payload[4] = afe_bridge_device_config;
        send_frame(bus, CAN_ID_AMS_AFE_STATUS, payload);
    }

    count = measurement->valid_cell_count;
    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_CELL_GROUPS) && count != 0U) {
        if (cursor->next_cell >= count) {
            cursor->next_cell = 0U;
        }
        memset(payload, 0xFF, sizeof(payload));
        for (index = 0U; index < 4U &&
            (uint16_t)(cursor->next_cell + index) < count; index++) {
            put_u16(&payload[index * 2U],
                (uint16_t)measurement->cell_voltage_mv[
                    cursor->next_cell + index]);
        }
        send_frame(bus, (uint16_t)(CAN_ID_AMS_CELL_GROUP_BASE +
            cursor->next_cell / 4U), payload);
        cursor->next_cell = (uint16_t)(cursor->next_cell + 4U);
    }

    count = measurement->valid_temperature_count;
    if (CAN_BMS_MESSAGE_ENABLED(CAN_BMS_MSG_TEMP_GROUPS) && count != 0U) {
        if (cursor->next_temperature >= count) {
            cursor->next_temperature = 0U;
        }
        memset(payload, 0xFF, sizeof(payload));
        for (index = 0U; index < 4U &&
            (uint16_t)(cursor->next_temperature + index) < count; index++) {
            put_u16(&payload[index * 2U],
                (uint16_t)measurement->temperature_dc[
                    cursor->next_temperature + index]);
        }
        send_frame(bus, (uint16_t)(CAN_ID_AMS_TEMP_GROUP_BASE +
            cursor->next_temperature / 4U), payload);
        cursor->next_temperature =
            (uint16_t)(cursor->next_temperature + 4U);
    }
}
