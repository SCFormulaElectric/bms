#ifndef AMS_BATTERY_H
#define AMS_BATTERY_H

#include <stdint.h>
#include "Ams/ams_types.h"

typedef struct {
    int64_t remaining_charge_mas;
    uint32_t last_update_ms;
    uint32_t rest_started_ms;
    uint16_t soc_permille;
    uint16_t minimum_cell_mv;
    uint16_t maximum_cell_mv;
    uint16_t minimum_cell_index;
    uint16_t maximum_cell_index;
    int16_t minimum_temperature_dc;
    int16_t maximum_temperature_dc;
    uint8_t initialized;
    uint8_t empty_anchor_active;
    uint8_t full_anchor_active;
    uint8_t rested;
} ams_battery_state_t;

void ams_battery_initialize(ams_battery_state_t *state,
    const ams_config_t *config, uint16_t initial_soc_permille);
void ams_battery_update(ams_battery_state_t *state,
    const ams_config_t *config, const ams_measurement_t *measurement,
    uint32_t now_ms);
uint16_t ams_battery_ocv_soc(const ams_config_t *config,
    uint16_t minimum_cell_mv);

#endif /* AMS_BATTERY_H */
