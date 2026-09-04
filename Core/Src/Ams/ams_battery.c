#include "Ams/ams_battery.h"

#include <stddef.h>
#include <string.h>

static int64_t capacity_mas(const ams_config_t *config)
{
    return (config == NULL) ? 0 : (int64_t)config->capacity_mah * 3600;
}

uint16_t ams_battery_ocv_soc(const ams_config_t *config,
    uint16_t minimum_cell_mv)
{
    uint32_t index;

    if (config == NULL ||
        minimum_cell_mv <= config->soc_drift_points[0].cell_mv) {
        return 0U;
    }
    for (index = 1U; index < AMS_SOC_DRIFT_POINT_COUNT; index++) {
        const ams_soc_drift_point_t *lower =
            &config->soc_drift_points[index - 1U];
        const ams_soc_drift_point_t *upper =
            &config->soc_drift_points[index];
        if (minimum_cell_mv <= upper->cell_mv) {
            const uint32_t voltage_span = upper->cell_mv - lower->cell_mv;
            const uint32_t soc_span =
                upper->soc_permille - lower->soc_permille;
            return (uint16_t)(lower->soc_permille +
                ((uint32_t)(minimum_cell_mv - lower->cell_mv) * soc_span) /
                    voltage_span);
        }
    }
    return 1000U;
}

void ams_battery_initialize(ams_battery_state_t *state,
    const ams_config_t *config, uint16_t initial_soc_permille)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    if (initial_soc_permille > 1000U) {
        initial_soc_permille = 1000U;
    }
    state->soc_permille = initial_soc_permille;
    state->remaining_charge_mas =
        capacity_mas(config) * initial_soc_permille / 1000;
}

void ams_battery_update(ams_battery_state_t *state,
    const ams_config_t *config, const ams_measurement_t *measurement,
    uint32_t now_ms)
{
    uint16_t index;
    int64_t capacity;

    if (state == NULL || config == NULL || measurement == NULL ||
        measurement->status != AMS_SAMPLE_VALID ||
        measurement->valid_cell_count == 0U ||
        measurement->valid_temperature_count == 0U) {
        return;
    }
    capacity = capacity_mas(config);
    if (capacity <= 0) {
        return;
    }

    state->minimum_cell_mv = (uint16_t)measurement->cell_voltage_mv[0];
    state->maximum_cell_mv = state->minimum_cell_mv;
    state->minimum_cell_index = 0U;
    state->maximum_cell_index = 0U;
    for (index = 1U; index < measurement->valid_cell_count; index++) {
        const uint16_t value = (uint16_t)measurement->cell_voltage_mv[index];
        if (value < state->minimum_cell_mv) {
            state->minimum_cell_mv = value;
            state->minimum_cell_index = index;
        }
        if (value > state->maximum_cell_mv) {
            state->maximum_cell_mv = value;
            state->maximum_cell_index = index;
        }
    }
    state->minimum_temperature_dc = measurement->temperature_dc[0];
    state->maximum_temperature_dc = measurement->temperature_dc[0];
    for (index = 1U; index < measurement->valid_temperature_count; index++) {
        if (measurement->temperature_dc[index] <
            state->minimum_temperature_dc) {
            state->minimum_temperature_dc =
                measurement->temperature_dc[index];
        }
        if (measurement->temperature_dc[index] >
            state->maximum_temperature_dc) {
            state->maximum_temperature_dc =
                measurement->temperature_dc[index];
        }
    }

    if (state->initialized != 0U) {
        const uint32_t elapsed_ms = (uint32_t)(now_ms -
            state->last_update_ms);
        state->remaining_charge_mas -=
            (int64_t)measurement->pack_current_ma * elapsed_ms / 1000;
    } else {
        state->initialized = 1U;
    }
    state->last_update_ms = now_ms;
    if (state->remaining_charge_mas < 0) {
        state->remaining_charge_mas = 0;
    } else if (state->remaining_charge_mas > capacity) {
        state->remaining_charge_mas = capacity;
    }

    state->empty_anchor_active =
        (state->minimum_cell_mv <= AMS_CELL_SOC_EMPTY_MV) ? 1U : 0U;
    state->full_anchor_active =
        (state->minimum_cell_mv >= config->cell_soc_full_mv &&
         measurement->pack_current_ma <= 0) ? 1U : 0U;
    if (state->empty_anchor_active != 0U) {
        state->remaining_charge_mas = 0;
    } else if (state->full_anchor_active != 0U) {
        state->remaining_charge_mas = capacity;
    } else if ((uint64_t)((measurement->pack_current_ma < 0) ?
            -(int64_t)measurement->pack_current_ma :
            measurement->pack_current_ma) <=
            config->soc_drift_rest_current_ma) {
        if (state->rested == 0U) {
            state->rest_started_ms = now_ms;
            state->rested = 1U;
        }
        if ((uint32_t)(now_ms - state->rest_started_ms) >=
            config->soc_drift_rest_ms) {
            const uint16_t ocv_soc =
                ams_battery_ocv_soc(config, state->minimum_cell_mv);
            state->remaining_charge_mas = capacity * ocv_soc / 1000;
        }
    } else {
        state->rested = 0U;
        state->rest_started_ms = 0U;
    }
    state->soc_permille =
        (uint16_t)(state->remaining_charge_mas * 1000 / capacity);
}
