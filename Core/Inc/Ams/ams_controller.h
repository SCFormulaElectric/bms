#ifndef AMS_CONTROLLER_H
#define AMS_CONTROLLER_H

#include <stdint.h>
#include "Ams/ams_types.h"

typedef struct {
    uint32_t voltage_high_since[AMS_MAX_CELLS];
    uint32_t voltage_low_since[AMS_MAX_CELLS];
    uint32_t temperature_high_since[AMS_MAX_TEMPERATURES];
    uint32_t temperature_low_since[AMS_MAX_TEMPERATURES];
    uint32_t overcurrent_since;
    uint8_t voltage_high_active[AMS_MAX_CELLS];
    uint8_t voltage_low_active[AMS_MAX_CELLS];
    uint8_t temperature_high_active[AMS_MAX_TEMPERATURES];
    uint8_t temperature_low_active[AMS_MAX_TEMPERATURES];
    uint8_t overcurrent_active;
    ams_decision_t decision;
} ams_controller_t;

void ams_controller_initialize(ams_controller_t *controller);
void ams_controller_step(ams_controller_t *controller,
    const ams_measurement_t *measurement, uint32_t immediate_faults,
    uint32_t now_ms);

#endif /* AMS_CONTROLLER_H */
