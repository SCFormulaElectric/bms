#ifndef AMS_TYPES_H
#define AMS_TYPES_H

#include <stdint.h>
#include "Ams/ams_config.h"

typedef enum {
    AMS_STATE_INIT = 0,
    AMS_STATE_SELF_TEST,
    AMS_STATE_STANDBY,
    AMS_STATE_PRECHARGE,
    AMS_STATE_ACTIVE,
    AMS_STATE_CHARGING,
    AMS_STATE_BALANCING,
    AMS_STATE_FAULT_LATCHED,
    AMS_STATE_SHUTDOWN
} ams_state_t;

typedef enum {
    AMS_SAMPLE_UNAVAILABLE = 0,
    AMS_SAMPLE_VALID
} ams_sample_status_t;

typedef struct {
    int32_t cell_voltage_mv[AMS_MAX_CELLS];
    int16_t temperature_dc[AMS_MAX_TEMPERATURES];
    int32_t pack_current_ma;
    int32_t pack_voltage_mv;
    uint32_t timestamp_ms;
    uint16_t valid_cell_count;
    uint16_t valid_temperature_count;
    ams_sample_status_t status;
} ams_measurement_t;

typedef enum {
    AMS_FAULT_NONE                  = 0U,
    AMS_FAULT_NOT_COMMISSIONED      = (1UL << 0),
    AMS_FAULT_AFE_COMMUNICATION     = (1UL << 1),
    AMS_FAULT_MEASUREMENT_STALE     = (1UL << 2),
    AMS_FAULT_CELL_OVERVOLTAGE      = (1UL << 3),
    AMS_FAULT_CELL_UNDERVOLTAGE     = (1UL << 4),
    AMS_FAULT_OVERTEMPERATURE       = (1UL << 5),
    AMS_FAULT_UNDERTEMPERATURE      = (1UL << 6),
    AMS_FAULT_DISCHARGE_OVERCURRENT = (1UL << 7),
    AMS_FAULT_SENSOR_PLAUSIBILITY   = (1UL << 8),
    AMS_FAULT_INTERNAL              = (1UL << 9),
    AMS_FAULT_CHARGE_OVERCURRENT    = (1UL << 10),
    AMS_FAULT_CONFIGURATION         = (1UL << 11),
    AMS_FAULT_CURRENT_SENSOR        = (1UL << 12)
} ams_fault_t;

typedef struct {
    uint32_t bits;
    uint32_t timestamp_ms;
    int32_t measured_value;
    int32_t threshold_value;
    uint16_t segment;
    uint16_t channel;
} ams_fault_record_t;

typedef struct {
    ams_state_t state;
    uint32_t active_faults;
    uint32_t latched_faults;
    uint8_t discharge_enable_request;
    uint8_t charge_enable_request;
    uint8_t fan_enable_request;
    ams_fault_record_t first_fault;
} ams_decision_t;

#endif /* AMS_TYPES_H */
