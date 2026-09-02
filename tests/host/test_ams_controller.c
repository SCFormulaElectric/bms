#include <assert.h>
#include <string.h>

#include "Ams/ams_controller.h"

static ams_measurement_t nominal_measurement(void)
{
    ams_measurement_t measurement;
    uint16_t index;

    memset(&measurement, 0, sizeof(measurement));
    measurement.status = AMS_SAMPLE_VALID;
    measurement.valid_cell_count = AMS_MAX_CELLS;
    measurement.valid_temperature_count = AMS_MAX_TEMPERATURES;
    for (index = 0U; index < AMS_MAX_CELLS; index++) {
        measurement.cell_voltage_mv[index] = 3700;
    }
    for (index = 0U; index < AMS_MAX_TEMPERATURES; index++) {
        measurement.temperature_dc[index] = 250;
    }
    return measurement;
}

static void test_invalid_measurement_fails_closed(void)
{
    ams_controller_t controller;
    ams_measurement_t measurement = nominal_measurement();

    ams_controller_initialize(&controller);
    measurement.status = AMS_SAMPLE_UNAVAILABLE;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 10U);
    assert((controller.decision.latched_faults &
        AMS_FAULT_AFE_COMMUNICATION) != 0U);
    assert(controller.decision.state == AMS_STATE_FAULT_LATCHED);
    assert(controller.decision.shutdown_closed_request == 0U);
}

static void test_voltage_fault_persistence(void)
{
    ams_controller_t controller;
    ams_measurement_t measurement = nominal_measurement();

    ams_controller_initialize(&controller);
    measurement.cell_voltage_mv[7] = AMS_CELL_OVERVOLTAGE_MV + 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 100U);
    assert((controller.decision.active_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) == 0U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE,
        100U + AMS_VOLTAGE_CURRENT_PERSIST_MS - 1U);
    assert((controller.decision.active_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) == 0U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE,
        100U + AMS_VOLTAGE_CURRENT_PERSIST_MS);
    assert((controller.decision.latched_faults &
        AMS_FAULT_CELL_OVERVOLTAGE) != 0U);
    assert(controller.decision.first_fault.channel == 7U);
}

static void test_transient_voltage_violation_clears_debounce(void)
{
    ams_controller_t controller;
    ams_measurement_t measurement = nominal_measurement();

    ams_controller_initialize(&controller);
    measurement.cell_voltage_mv[0] = AMS_CELL_UNDERVOLTAGE_MV - 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 0U);
    measurement.cell_voltage_mv[0] = 3700;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 400U);
    measurement.cell_voltage_mv[0] = AMS_CELL_UNDERVOLTAGE_MV - 1;
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 500U);
    ams_controller_step(&controller, &measurement, AMS_FAULT_NONE, 900U);
    assert((controller.decision.latched_faults &
        AMS_FAULT_CELL_UNDERVOLTAGE) == 0U);
}

int main(void)
{
    test_invalid_measurement_fails_closed();
    test_voltage_fault_persistence();
    test_transient_voltage_violation_clears_debounce();
    return 0;
}
