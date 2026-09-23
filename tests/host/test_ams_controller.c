#include <assert.h>
#include <string.h>

#include "Ams/ams_controller.h"

static ams_config_t nominal_config(void)
{
    ams_config_t config;
    ams_config_load_defaults(&config);
    config.measurement_max_age_ms = 1000U;
    return config;
}

static ams_measurement_t nominal_measurement(const ams_config_t *config)
{
    ams_measurement_t measurement;
    uint16_t index;

    memset(&measurement, 0, sizeof(measurement));
    measurement.status = AMS_SAMPLE_VALID;
    measurement.valid_cell_count = ams_config_cell_count(config);
    measurement.valid_temperature_count =
        ams_config_temperature_count(config);
    for (index = 0U; index < measurement.valid_cell_count; index++) {
        measurement.cell_voltage_mv[index] = 3700;
    }
    for (index = 0U; index < measurement.valid_temperature_count; index++) {
        measurement.temperature_dc[index] = 250;
    }
    return measurement;
}

static void test_invalid_measurement_fails_closed(void)
{
    ams_controller_t controller;
    ams_config_t config = nominal_config();
    ams_measurement_t measurement = nominal_measurement(&config);

    ams_controller_initialize(&controller, &config);
    measurement.status = AMS_SAMPLE_UNAVAILABLE;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 10U);
    assert((controller.decision.latched_faults &
        AMS_FAULT_AFE_COMMUNICATION) != 0U);
    assert(controller.decision.state == AMS_STATE_FAULT_LATCHED);
    assert(controller.decision.discharge_enable_request == 0U);
    assert(controller.decision.charge_enable_request == 0U);
}

static void test_voltage_fault_persistence(void)
{
    ams_controller_t controller;
    ams_config_t config = nominal_config();
    ams_measurement_t measurement = nominal_measurement(&config);

    ams_controller_initialize(&controller, &config);
    measurement.cell_voltage_mv[7] = config.cell_overvoltage_mv + 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 100U);
    assert((controller.decision.active_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) == 0U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE,
        100U + config.voltage_current_persist_ms - 1U);
    assert((controller.decision.active_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) == 0U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE,
        100U + config.voltage_current_persist_ms);
    assert((controller.decision.latched_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) != 0U);
    assert(controller.decision.first_fault.channel == 7U);
}

static void test_transient_voltage_violation_clears_debounce(void)
{
    ams_controller_t controller;
    ams_config_t config = nominal_config();
    ams_measurement_t measurement = nominal_measurement(&config);

    ams_controller_initialize(&controller, &config);
    measurement.cell_voltage_mv[0] = config.cell_undervoltage_mv - 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 0U);
    measurement.cell_voltage_mv[0] = 3700;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 400U);
    measurement.cell_voltage_mv[0] = config.cell_undervoltage_mv - 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 500U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 900U);
    assert((controller.decision.latched_faults &
        AMS_FAULT_CELL_UNDERVOLTAGE) == 0U);
}

static void test_clear_faults_resets_controller_and_recovers(void)
{
    ams_controller_t controller;
    ams_config_t config = nominal_config();
    ams_measurement_t measurement = nominal_measurement(&config);

    ams_controller_initialize(&controller, &config);
    measurement.cell_voltage_mv[7] = config.cell_overvoltage_mv + 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 100U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE,
        100U + config.voltage_current_persist_ms);
    assert(controller.decision.state == AMS_STATE_FAULT_LATCHED);
    assert(controller.decision.first_fault.bits ==
        AMS_FAULT_CELL_OVERVOLTAGE);

    ams_controller_clear_faults(&controller);
    assert(controller.decision.state == AMS_STATE_INIT);
    assert(controller.decision.active_faults == AMS_FAULT_NONE);
    assert(controller.decision.latched_faults == AMS_FAULT_NONE);
    assert(controller.decision.first_fault.bits == AMS_FAULT_NONE);
    assert(controller.voltage_high_active[7] == 0U);
    assert(controller.voltage_high_since[7] == 0U);
    assert(controller.decision.discharge_enable_request == 0U);
    assert(controller.decision.charge_enable_request == 0U);
    assert(controller.decision.fan_enable_request == 0U);

    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 700U);
    assert(controller.decision.state == AMS_STATE_FAULT_LATCHED);
    assert((controller.decision.latched_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) != 0U);

    ams_controller_clear_faults(&controller);
    measurement.cell_voltage_mv[7] = 3700;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 800U);
    assert(controller.decision.state == AMS_STATE_SELF_TEST);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 900U);
    assert(controller.decision.state == AMS_STATE_STANDBY);
}

int main(void)
{
    test_invalid_measurement_fails_closed();
    test_voltage_fault_persistence();
    test_transient_voltage_violation_clears_debounce();
    test_clear_faults_resets_controller_and_recovers();
    return 0;
}
