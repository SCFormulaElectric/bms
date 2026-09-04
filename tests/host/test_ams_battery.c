#include <assert.h>
#include <string.h>

#include "Ams/ams_battery.h"

static ams_measurement_t measurement_for(const ams_config_t *config,
    int32_t cell_mv, int32_t current_ma)
{
    ams_measurement_t measurement;
    uint16_t index;

    memset(&measurement, 0, sizeof(measurement));
    measurement.status = AMS_SAMPLE_VALID;
    measurement.valid_cell_count = ams_config_cell_count(config);
    measurement.valid_temperature_count =
        ams_config_temperature_count(config);
    measurement.pack_current_ma = current_ma;
    for (index = 0U; index < measurement.valid_cell_count; index++) {
        measurement.cell_voltage_mv[index] = cell_mv;
    }
    for (index = 0U; index < measurement.valid_temperature_count; index++) {
        measurement.temperature_dc[index] = 250;
    }
    return measurement;
}

static void test_three_volts_forces_empty(void)
{
    ams_config_t config;
    ams_battery_state_t battery;
    ams_measurement_t measurement;

    ams_config_load_defaults(&config);
    ams_battery_initialize(&battery, &config, 800U);
    measurement = measurement_for(&config, 3700, 0);
    measurement.cell_voltage_mv[3] = AMS_CELL_SOC_EMPTY_MV;
    ams_battery_update(&battery, &config, &measurement, 10U);
    assert(battery.soc_permille == 0U);
    assert(battery.empty_anchor_active != 0U);
}

static void test_coulomb_counting_and_polarity(void)
{
    ams_config_t config;
    ams_battery_state_t battery;
    ams_measurement_t measurement;
    const int32_t normal_current = 1000;

    ams_config_load_defaults(&config);
    config.capacity_mah = 1000U;
    config.soc_drift_rest_current_ma = 500U;
    ams_battery_initialize(&battery, &config, 1000U);
    measurement = measurement_for(&config, 3800, normal_current);
    ams_battery_update(&battery, &config, &measurement, 0U);
    ams_battery_update(&battery, &config, &measurement, 360000U);
    assert(battery.soc_permille == 900U);
    config.current_sensor_polarity = AMS_CURRENT_POLARITY_INVERTED;
    assert(ams_config_convert_current_ma(&config, 3048U) == -1000);
}

int main(void)
{
    test_three_volts_forces_empty();
    test_coulomb_counting_and_polarity();
    return 0;
}
