#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "Peripherals/bms_summary_config.h"

static void test_default_frames_match_version_one(void)
{
    bms_summary_config_t config;
    bms_summary_values_t values = {0};
    uint8_t data[8];

    bms_summary_config_load_defaults(&config);
    assert(bms_summary_config_is_valid(&config) != 0U);
    assert(config.active_count == 2U);
    values.state = 3U;
    values.soc_permille = 777U;
    values.pack_voltage_mv = 401200;
    values.pack_current_ma = -12300;
    values.minimum_cell_mv = 3456U;
    values.maximum_cell_mv = 4123U;
    values.maximum_temperature_dc = 567;
    values.active_faults = 0x12345678UL;

    assert(bms_summary_pack_frame(&config, 0U, &values, data) != 0U);
    assert(data[0] == BMS_CAN_PROTOCOL_VERSION);
    assert(data[1] == 3U);
    assert(data[2] == 0x09U && data[3] == 0x03U);
    assert(data[4] == 0xACU && data[5] == 0x0FU);
    assert(data[6] == 0x85U && data[7] == 0xFFU);

    assert(bms_summary_pack_frame(&config, 1U, &values, data) != 0U);
    assert(data[0] == 0x80U && data[1] == 0x0DU);
    assert(data[2] == 0x1BU && data[3] == 0x10U);
    assert(data[4] == 0x37U && data[5] == 0x02U);
    assert(data[6] == 0x78U && data[7] == 0x56U);
}

static void test_descriptor_round_trip(void)
{
    bms_summary_slot_t source = {{
        BMS_CAN_SIGNAL_STATE, BMS_CAN_SIGNAL_SOC_PERMILLE,
        BMS_CAN_SIGNAL_ACTIVE_FAULTS, 0U, 0U, 0U, 0U, 0U
    }};
    bms_summary_slot_t decoded = {{0}};

    bms_summary_unpack_descriptor(bms_summary_pack_descriptor(&source),
        &decoded);
    assert(memcmp(&source, &decoded, sizeof(source)) == 0);
}

static void test_limits_and_invalid_layouts(void)
{
    bms_summary_config_t config;

    bms_summary_config_load_defaults(&config);
    config.active_count = BMS_CAN_SUMMARY_MAX_FRAMES;
    for (uint32_t index = 2U; index < BMS_CAN_SUMMARY_MAX_FRAMES; index++) {
        config.slots[index].signals[0] = BMS_CAN_SIGNAL_STATE;
    }
    assert(bms_summary_config_is_valid(&config) != 0U);
    config.active_count = BMS_CAN_SUMMARY_MAX_FRAMES + 1U;
    assert(bms_summary_config_is_valid(&config) == 0U);

    bms_summary_config_load_defaults(&config);
    config.slots[0].signals[0] = BMS_CAN_SIGNAL_ACTIVE_FAULTS;
    config.slots[0].signals[1] = BMS_CAN_SIGNAL_LATCHED_FAULTS;
    config.slots[0].signals[2] = BMS_CAN_SIGNAL_STATE;
    assert(bms_summary_config_is_valid(&config) == 0U);

    bms_summary_config_load_defaults(&config);
    config.slots[0].signals[1] = BMS_CAN_SIGNAL_NONE;
    config.slots[0].signals[2] = BMS_CAN_SIGNAL_STATE;
    assert(bms_summary_config_is_valid(&config) == 0U);
}

int main(void)
{
    test_default_frames_match_version_one();
    test_descriptor_round_trip();
    test_limits_and_invalid_layouts();
    return 0;
}
