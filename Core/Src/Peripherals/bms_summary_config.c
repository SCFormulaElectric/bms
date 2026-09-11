#include "Peripherals/bms_summary_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static void put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
}

static void put_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8);
    destination[2] = (uint8_t)(value >> 16);
    destination[3] = (uint8_t)(value >> 24);
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

uint8_t bms_summary_signal_width(uint8_t signal)
{
    switch (signal) {
        case BMS_CAN_SIGNAL_FORMAT_VERSION:
        case BMS_CAN_SIGNAL_STATE:
        case BMS_CAN_SIGNAL_OUTPUT_REQUESTS:
            return 1U;
        case BMS_CAN_SIGNAL_SOC_PERMILLE:
        case BMS_CAN_SIGNAL_PACK_VOLTAGE_100MV:
        case BMS_CAN_SIGNAL_PACK_CURRENT_100MA:
        case BMS_CAN_SIGNAL_MIN_CELL_MV:
        case BMS_CAN_SIGNAL_MAX_CELL_MV:
        case BMS_CAN_SIGNAL_MAX_TEMPERATURE_DC:
        case BMS_CAN_SIGNAL_ACTIVE_FAULTS_LOW16:
        case BMS_CAN_SIGNAL_VALID_CELL_COUNT:
        case BMS_CAN_SIGNAL_VALID_TEMPERATURE_COUNT:
            return 2U;
        case BMS_CAN_SIGNAL_ACTIVE_FAULTS:
        case BMS_CAN_SIGNAL_LATCHED_FAULTS:
            return 4U;
        default:
            return 0U;
    }
}

void bms_summary_config_load_defaults(bms_summary_config_t *config)
{
    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->period_ms = BMS_CAN_SUMMARY_PERIOD_MS;
    config->active_count = 2U;
    config->generation = 1U;

    config->slots[0].signals[0] = BMS_CAN_SIGNAL_FORMAT_VERSION;
    config->slots[0].signals[1] = BMS_CAN_SIGNAL_STATE;
    config->slots[0].signals[2] = BMS_CAN_SIGNAL_SOC_PERMILLE;
    config->slots[0].signals[3] = BMS_CAN_SIGNAL_PACK_VOLTAGE_100MV;
    config->slots[0].signals[4] = BMS_CAN_SIGNAL_PACK_CURRENT_100MA;

    config->slots[1].signals[0] = BMS_CAN_SIGNAL_MIN_CELL_MV;
    config->slots[1].signals[1] = BMS_CAN_SIGNAL_MAX_CELL_MV;
    config->slots[1].signals[2] = BMS_CAN_SIGNAL_MAX_TEMPERATURE_DC;
    config->slots[1].signals[3] = BMS_CAN_SIGNAL_ACTIVE_FAULTS_LOW16;
}

uint8_t bms_summary_config_is_valid(const bms_summary_config_t *config)
{
    uint32_t slot_index;

    if (config == NULL || config->period_ms != BMS_CAN_SUMMARY_PERIOD_MS ||
        config->active_count > BMS_CAN_SUMMARY_MAX_FRAMES) {
        return 0U;
    }
    for (slot_index = 0U; slot_index < config->active_count; slot_index++) {
        uint32_t signal_index;
        uint32_t packed_bytes = 0U;
        uint8_t terminated = 0U;

        for (signal_index = 0U;
            signal_index < BMS_CAN_SUMMARY_MAX_SIGNALS; signal_index++) {
            const uint8_t signal = config->slots[slot_index].signals[signal_index];
            const uint8_t width = bms_summary_signal_width(signal);

            if (signal == BMS_CAN_SIGNAL_NONE) {
                terminated = 1U;
                continue;
            }
            if (terminated != 0U || width == 0U ||
                packed_bytes + width > BMS_CAN_FRAME_DLC) {
                return 0U;
            }
            packed_bytes += width;
        }
        if (packed_bytes == 0U) {
            return 0U;
        }
    }
    return 1U;
}

uint32_t bms_summary_pack_descriptor(const bms_summary_slot_t *slot)
{
    uint32_t descriptor = 0U;
    uint32_t index;

    if (slot == NULL) {
        return 0U;
    }
    for (index = 0U; index < BMS_CAN_SUMMARY_MAX_SIGNALS; index++) {
        descriptor |= ((uint32_t)slot->signals[index] & 0x0FU) << (index * 4U);
    }
    return descriptor;
}

void bms_summary_unpack_descriptor(uint32_t descriptor,
    bms_summary_slot_t *slot)
{
    uint32_t index;

    if (slot == NULL) {
        return;
    }
    for (index = 0U; index < BMS_CAN_SUMMARY_MAX_SIGNALS; index++) {
        slot->signals[index] = (uint8_t)((descriptor >> (index * 4U)) & 0x0FU);
    }
}

static void pack_signal(uint8_t signal, const bms_summary_values_t *values,
    uint8_t *destination)
{
    switch (signal) {
        case BMS_CAN_SIGNAL_FORMAT_VERSION:
            destination[0] = BMS_CAN_PROTOCOL_VERSION;
            break;
        case BMS_CAN_SIGNAL_STATE:
            destination[0] = values->state;
            break;
        case BMS_CAN_SIGNAL_SOC_PERMILLE:
            put_u16(destination, values->soc_permille);
            break;
        case BMS_CAN_SIGNAL_PACK_VOLTAGE_100MV:
            put_u16(destination, clamp_u16(values->pack_voltage_mv / 100));
            break;
        case BMS_CAN_SIGNAL_PACK_CURRENT_100MA:
            put_u16(destination,
                (uint16_t)clamp_i16(values->pack_current_ma / 100));
            break;
        case BMS_CAN_SIGNAL_MIN_CELL_MV:
            put_u16(destination, values->minimum_cell_mv);
            break;
        case BMS_CAN_SIGNAL_MAX_CELL_MV:
            put_u16(destination, values->maximum_cell_mv);
            break;
        case BMS_CAN_SIGNAL_MAX_TEMPERATURE_DC:
            put_u16(destination, (uint16_t)values->maximum_temperature_dc);
            break;
        case BMS_CAN_SIGNAL_ACTIVE_FAULTS_LOW16:
            put_u16(destination, (uint16_t)values->active_faults);
            break;
        case BMS_CAN_SIGNAL_ACTIVE_FAULTS:
            put_u32(destination, values->active_faults);
            break;
        case BMS_CAN_SIGNAL_LATCHED_FAULTS:
            put_u32(destination, values->latched_faults);
            break;
        case BMS_CAN_SIGNAL_OUTPUT_REQUESTS:
            destination[0] = (uint8_t)(values->discharge_enable_request |
                (values->charge_enable_request << 1U) |
                (values->fan_enable_request << 2U));
            break;
        case BMS_CAN_SIGNAL_VALID_CELL_COUNT:
            put_u16(destination, values->valid_cell_count);
            break;
        case BMS_CAN_SIGNAL_VALID_TEMPERATURE_COUNT:
            put_u16(destination, values->valid_temperature_count);
            break;
        default:
            break;
    }
}

uint8_t bms_summary_pack_frame(const bms_summary_config_t *config,
    uint8_t slot_index, const bms_summary_values_t *values, uint8_t data[8])
{
    uint32_t signal_index;
    uint32_t offset = 0U;

    if (config == NULL || values == NULL || data == NULL ||
        bms_summary_config_is_valid(config) == 0U ||
        slot_index >= config->active_count) {
        return 0U;
    }
    memset(data, 0, BMS_CAN_FRAME_DLC);
    for (signal_index = 0U;
        signal_index < BMS_CAN_SUMMARY_MAX_SIGNALS; signal_index++) {
        const uint8_t signal = config->slots[slot_index].signals[signal_index];
        const uint8_t width = bms_summary_signal_width(signal);

        if (signal == BMS_CAN_SIGNAL_NONE) {
            break;
        }
        pack_signal(signal, values, &data[offset]);
        offset += width;
    }
    return 1U;
}
