#ifndef BMS_SUMMARY_CONFIG_H
#define BMS_SUMMARY_CONFIG_H

#include <stdint.h>

#include "Peripherals/bms_can_protocol.h"

typedef struct {
    uint8_t signals[BMS_CAN_SUMMARY_MAX_SIGNALS];
} bms_summary_slot_t;

typedef struct {
    uint16_t period_ms;
    uint8_t active_count;
    uint8_t generation;
    bms_summary_slot_t slots[BMS_CAN_SUMMARY_MAX_FRAMES];
} bms_summary_config_t;

typedef struct {
    uint8_t state;
    uint16_t soc_permille;
    int32_t pack_voltage_mv;
    int32_t pack_current_ma;
    uint16_t minimum_cell_mv;
    uint16_t maximum_cell_mv;
    int16_t maximum_temperature_dc;
    uint32_t active_faults;
    uint32_t latched_faults;
    uint8_t discharge_enable_request;
    uint8_t charge_enable_request;
    uint8_t fan_enable_request;
    uint16_t valid_cell_count;
    uint16_t valid_temperature_count;
} bms_summary_values_t;

void bms_summary_config_load_defaults(bms_summary_config_t *config);
uint8_t bms_summary_config_is_valid(const bms_summary_config_t *config);
uint8_t bms_summary_signal_width(uint8_t signal);
uint32_t bms_summary_pack_descriptor(const bms_summary_slot_t *slot);
void bms_summary_unpack_descriptor(uint32_t descriptor,
    bms_summary_slot_t *slot);
uint8_t bms_summary_pack_frame(const bms_summary_config_t *config,
    uint8_t slot_index, const bms_summary_values_t *values, uint8_t data[8]);

#endif /* BMS_SUMMARY_CONFIG_H */
