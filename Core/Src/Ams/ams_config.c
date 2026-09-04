#include "Ams/ams_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

void ams_config_load_defaults(ams_config_t *config)
{
    static const ams_soc_drift_point_t points[AMS_SOC_DRIFT_POINT_COUNT] = {
        {3000U, 0U}, {3400U, 100U}, {3700U, 500U},
        {4000U, 850U}, {4200U, 1000U}
    };

    if (config == NULL) {
        return;
    }
    memset(config, 0, sizeof(*config));
    config->segment_count = 1U;
    config->cells_per_segment = 12U;
    config->temperatures_per_segment = 4U;
    config->current_adc_channel = 0U;
    config->current_sensor_polarity = AMS_CURRENT_POLARITY_NORMAL;
    config->cell_undervoltage_mv = 3000U;
    config->cell_overvoltage_mv = 4200U;
    config->cell_soc_full_mv = 4150U;
    config->cell_min_temperature_dc = 0;
    config->cell_max_temperature_dc = 600;
    config->fan_on_temperature_dc = 400;
    config->discharge_current_limit_ma = 300000U;
    config->charge_current_limit_ma = 100000U;
    config->capacity_mah = 5000U;
    config->current_adc_zero_count = 2048U;
    /* Engineering placeholder; replace with measured installed-sensor gain. */
    config->current_gain_ua_per_count = 1000U;
    config->voltage_current_persist_ms = 500U;
    config->temperature_persist_ms = 1000U;
    config->measurement_max_age_ms = 100U;
    config->soc_drift_rest_current_ma = 1000U;
    config->soc_drift_rest_ms = 30000U;
    config->periodic_can_base_id = AMS_PERIODIC_CAN_DEFAULT_ID;
    config->periodic_can_frame_count = AMS_PERIODIC_CAN_DEFAULT_FRAMES;
    memcpy(config->soc_drift_points, points, sizeof(points));
}

uint16_t ams_config_cell_count(const ams_config_t *config)
{
    return (config == NULL) ? 0U :
        (uint16_t)config->segment_count * config->cells_per_segment;
}

uint16_t ams_config_temperature_count(const ams_config_t *config)
{
    return (config == NULL) ? 0U :
        (uint16_t)config->segment_count * config->temperatures_per_segment;
}

uint8_t ams_config_is_valid(const ams_config_t *config)
{
    uint32_t index;

    if (config == NULL || config->segment_count == 0U ||
        config->segment_count > AMS_MAX_SEGMENTS ||
        config->cells_per_segment < 6U ||
        config->cells_per_segment > AMS_MAX_CELLS_PER_SEGMENT ||
        config->temperatures_per_segment == 0U ||
        config->temperatures_per_segment > AMS_MAX_TEMPS_PER_SEGMENT ||
        config->current_adc_channel >= 2U ||
        (config->current_sensor_polarity != AMS_CURRENT_POLARITY_NORMAL &&
         config->current_sensor_polarity != AMS_CURRENT_POLARITY_INVERTED) ||
        config->cell_undervoltage_mv < AMS_CELL_SOC_EMPTY_MV ||
        config->cell_overvoltage_mv <= config->cell_undervoltage_mv ||
        config->cell_soc_full_mv <= config->cell_undervoltage_mv ||
        config->cell_soc_full_mv > config->cell_overvoltage_mv ||
        config->cell_max_temperature_dc <= config->cell_min_temperature_dc ||
        config->fan_on_temperature_dc < config->cell_min_temperature_dc ||
        config->fan_on_temperature_dc > config->cell_max_temperature_dc ||
        config->discharge_current_limit_ma == 0U ||
        config->discharge_current_limit_ma > INT32_MAX ||
        config->charge_current_limit_ma == 0U ||
        config->charge_current_limit_ma > INT32_MAX ||
        config->capacity_mah == 0U || config->current_gain_ua_per_count == 0U ||
        config->voltage_current_persist_ms == 0U ||
        config->temperature_persist_ms == 0U ||
        config->measurement_max_age_ms == 0U ||
        config->periodic_can_frame_count < AMS_PERIODIC_CAN_MIN_FRAMES ||
        config->periodic_can_frame_count > AMS_PERIODIC_CAN_MAX_FRAMES ||
        config->periodic_can_base_id > 0x7FFU ||
        (config->periodic_can_frame_count == 2U &&
         config->periodic_can_base_id >= 0x7FFU)) {
        return 0U;
    }
    for (index = 0U; index < AMS_SOC_DRIFT_POINT_COUNT; index++) {
        if (config->soc_drift_points[index].soc_permille > 1000U ||
            (index > 0U &&
             (config->soc_drift_points[index].cell_mv <=
                  config->soc_drift_points[index - 1U].cell_mv ||
              config->soc_drift_points[index].soc_permille <
                  config->soc_drift_points[index - 1U].soc_permille))) {
            return 0U;
        }
    }
    return 1U;
}

int32_t ams_config_convert_current_ma(const ams_config_t *config,
    uint16_t adc_count)
{
    int64_t current_ma;

    if (config == NULL || config->current_gain_ua_per_count == 0U) {
        return 0;
    }
    current_ma = ((int32_t)adc_count - config->current_adc_zero_count) *
        (int64_t)config->current_gain_ua_per_count;
    current_ma *= config->current_sensor_polarity;
    current_ma /= 1000;
    if (current_ma > INT32_MAX) {
        return INT32_MAX;
    }
    if (current_ma < INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)current_ma;
}
