#ifndef AMS_CONFIG_H
#define AMS_CONFIG_H

#include <stdint.h>

/* The BQ79616 accepts 6-16 cells, while this daughterboard routes 12 cell
 * inputs and four thermistor inputs. Arrays use fixed worst-case storage;
 * active topology is selected at runtime. */
#define AMS_MAX_SEGMENTS                   8U
#define AMS_MAX_CELLS_PER_SEGMENT         12U
#define AMS_MAX_TEMPS_PER_SEGMENT          4U
#define AMS_MAX_CELLS                     (AMS_MAX_SEGMENTS * AMS_MAX_CELLS_PER_SEGMENT)
#define AMS_MAX_TEMPERATURES              (AMS_MAX_SEGMENTS * AMS_MAX_TEMPS_PER_SEGMENT)
#define AMS_SOC_DRIFT_POINT_COUNT          5U

#define AMS_CELL_SOC_EMPTY_MV           3000
#define AMS_CURRENT_POLARITY_NORMAL         1
#define AMS_CURRENT_POLARITY_INVERTED      -1

#define AMS_AFE_DMA_BUFFER_BYTES          256U
#define AMS_AFE_DMA_TIMEOUT_MS              5U
#define AMS_AFE_SERVICE_POLL_MS             1U
#define AMS_CRITICAL_CYCLE_PERIOD_MS        20U
#define AMS_PERIODIC_CAN_DEFAULT_ID       0x5F0U
#define AMS_PERIODIC_CAN_DEFAULT_FRAMES       2U
#define AMS_PERIODIC_CAN_MIN_FRAMES           1U
#define AMS_PERIODIC_CAN_MAX_FRAMES           2U

/* The PCB mapping is known, but energizing outputs remains locked until the
 * external 12 V interface polarity and shutdown behavior are bench-tested. */
#define AMS_HARDWARE_OUTPUTS_COMMISSIONED   0U

typedef struct {
    uint16_t cell_mv;
    uint16_t soc_permille;
} ams_soc_drift_point_t;

typedef struct {
    uint8_t segment_count;
    uint8_t cells_per_segment;
    uint8_t temperatures_per_segment;
    uint8_t current_adc_channel;
    int8_t current_sensor_polarity;
    uint16_t cell_undervoltage_mv;
    uint16_t cell_overvoltage_mv;
    uint16_t cell_soc_full_mv;
    int16_t cell_min_temperature_dc;
    int16_t cell_max_temperature_dc;
    int16_t fan_on_temperature_dc;
    uint32_t discharge_current_limit_ma;
    uint32_t charge_current_limit_ma;
    uint32_t capacity_mah;
    uint16_t current_adc_zero_count;
    uint32_t current_gain_ua_per_count;
    uint32_t voltage_current_persist_ms;
    uint32_t temperature_persist_ms;
    uint32_t measurement_max_age_ms;
    uint32_t soc_drift_rest_current_ma;
    uint32_t soc_drift_rest_ms;
    uint16_t periodic_can_base_id;
    uint8_t periodic_can_frame_count;
    ams_soc_drift_point_t soc_drift_points[AMS_SOC_DRIFT_POINT_COUNT];
} ams_config_t;

void ams_config_load_defaults(ams_config_t *config);
uint8_t ams_config_is_valid(const ams_config_t *config);
uint16_t ams_config_cell_count(const ams_config_t *config);
uint16_t ams_config_temperature_count(const ams_config_t *config);
int32_t ams_config_convert_current_ma(const ams_config_t *config,
    uint16_t adc_count);

#endif /* AMS_CONFIG_H */
