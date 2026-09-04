#ifndef AMS_CAN_TELEMETRY_H
#define AMS_CAN_TELEMETRY_H

#include <stdint.h>
#include "Ams/ams_battery.h"
#include "Peripherals/can_bus.h"

typedef struct {
    uint16_t next_cell;
    uint16_t next_temperature;
    uint8_t rolling_counter;
} ams_can_cursor_t;

void ams_can_cursor_reset(ams_can_cursor_t *cursor);
void ams_can_publish_snapshot(can_bus_t *bus, ams_can_cursor_t *cursor,
    const ams_config_t *config, const ams_measurement_t *measurement,
    const ams_battery_state_t *battery, const ams_decision_t *decision,
    uint8_t afe_bringup_status, uint8_t afe_bringup_failed_step,
    uint8_t afe_verified_devices, uint8_t afe_bridge_device_config);

#endif /* AMS_CAN_TELEMETRY_H */
