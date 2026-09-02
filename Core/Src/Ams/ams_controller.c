#include "Ams/ams_controller.h"

#include <stddef.h>
#include <string.h>

static uint8_t persistence_elapsed(uint8_t *active, uint32_t *since,
    uint8_t violation, uint32_t now_ms, uint32_t duration_ms)
{
    if (violation == 0U) {
        *active = 0U;
        *since = 0U;
        return 0U;
    }
    if (*active == 0U) {
        *active = 1U;
        *since = now_ms;
        return 0U;
    }
    return ((uint32_t)(now_ms - *since) >= duration_ms) ? 1U : 0U;
}

static void record_first_fault(ams_decision_t *decision, uint32_t bit,
    uint32_t now_ms, int32_t measured, int32_t threshold,
    uint16_t segment, uint16_t channel)
{
    if (decision->first_fault.bits != AMS_FAULT_NONE) {
        return;
    }
    decision->first_fault.bits = bit;
    decision->first_fault.timestamp_ms = now_ms;
    decision->first_fault.measured_value = measured;
    decision->first_fault.threshold_value = threshold;
    decision->first_fault.segment = segment;
    decision->first_fault.channel = channel;
}

void ams_controller_initialize(ams_controller_t *controller)
{
    if (controller == NULL) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    controller->decision.state = AMS_STATE_INIT;
}

void ams_controller_step(ams_controller_t *controller,
    const ams_measurement_t *measurement, uint32_t immediate_faults,
    uint32_t now_ms)
{
    uint32_t faults = immediate_faults;
    uint16_t index;

    if (controller == NULL) {
        return;
    }
    controller->decision.shutdown_closed_request = 0U;

    if (AMS_TOPOLOGY_COMMISSIONED == 0U) {
        faults |= AMS_FAULT_NOT_COMMISSIONED;
    }
    if (measurement == NULL || measurement->status != AMS_SAMPLE_VALID ||
        measurement->valid_cell_count != AMS_MAX_CELLS ||
        measurement->valid_temperature_count != AMS_MAX_TEMPERATURES) {
        faults |= AMS_FAULT_AFE_COMMUNICATION;
    } else {
        for (index = 0U; index < measurement->valid_cell_count; index++) {
            const int32_t voltage = measurement->cell_voltage_mv[index];
            if (persistence_elapsed(&controller->voltage_high_active[index],
                &controller->voltage_high_since[index],
                (voltage > AMS_CELL_OVERVOLTAGE_MV) ? 1U : 0U,
                now_ms, AMS_VOLTAGE_CURRENT_PERSIST_MS) != 0U) {
                faults |= AMS_FAULT_CELL_OVERVOLTAGE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_CELL_OVERVOLTAGE, now_ms, voltage,
                    AMS_CELL_OVERVOLTAGE_MV,
                    (uint16_t)(index / AMS_CELLS_PER_SEGMENT),
                    (uint16_t)(index % AMS_CELLS_PER_SEGMENT));
            }
            if (persistence_elapsed(&controller->voltage_low_active[index],
                &controller->voltage_low_since[index],
                (voltage < AMS_CELL_UNDERVOLTAGE_MV) ? 1U : 0U,
                now_ms, AMS_VOLTAGE_CURRENT_PERSIST_MS) != 0U) {
                faults |= AMS_FAULT_CELL_UNDERVOLTAGE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_CELL_UNDERVOLTAGE, now_ms, voltage,
                    AMS_CELL_UNDERVOLTAGE_MV,
                    (uint16_t)(index / AMS_CELLS_PER_SEGMENT),
                    (uint16_t)(index % AMS_CELLS_PER_SEGMENT));
            }
        }
        for (index = 0U; index < measurement->valid_temperature_count;
            index++) {
            const int32_t temperature = measurement->temperature_dc[index];
            if (persistence_elapsed(
                &controller->temperature_high_active[index],
                &controller->temperature_high_since[index],
                (temperature > AMS_CELL_MAX_TEMPERATURE_DC) ? 1U : 0U,
                now_ms, AMS_TEMPERATURE_PERSIST_MS) != 0U) {
                faults |= AMS_FAULT_OVERTEMPERATURE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_OVERTEMPERATURE, now_ms, temperature,
                    AMS_CELL_MAX_TEMPERATURE_DC,
                    (uint16_t)(index / AMS_TEMPS_PER_SEGMENT),
                    (uint16_t)(index % AMS_TEMPS_PER_SEGMENT));
            }
            if (persistence_elapsed(
                &controller->temperature_low_active[index],
                &controller->temperature_low_since[index],
                (temperature < AMS_CELL_MIN_TEMPERATURE_DC) ? 1U : 0U,
                now_ms, AMS_TEMPERATURE_PERSIST_MS) != 0U) {
                faults |= AMS_FAULT_UNDERTEMPERATURE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_UNDERTEMPERATURE, now_ms, temperature,
                    AMS_CELL_MIN_TEMPERATURE_DC,
                    (uint16_t)(index / AMS_TEMPS_PER_SEGMENT),
                    (uint16_t)(index % AMS_TEMPS_PER_SEGMENT));
            }
        }
        if (persistence_elapsed(&controller->overcurrent_active,
            &controller->overcurrent_since,
            (measurement->pack_current_ma > AMS_CELL_OVERCURRENT_MA ||
             measurement->pack_current_ma < -AMS_CELL_OVERCURRENT_MA) ? 1U : 0U,
            now_ms, AMS_VOLTAGE_CURRENT_PERSIST_MS) != 0U) {
            faults |= AMS_FAULT_OVERCURRENT;
            record_first_fault(&controller->decision,
                AMS_FAULT_OVERCURRENT, now_ms,
                measurement->pack_current_ma, AMS_CELL_OVERCURRENT_MA,
                0U, 0U);
        }
    }

    controller->decision.active_faults = faults;
    controller->decision.latched_faults |= faults;
    if (controller->decision.latched_faults != AMS_FAULT_NONE) {
        controller->decision.state = AMS_STATE_FAULT_LATCHED;
    } else if (controller->decision.state == AMS_STATE_INIT) {
        controller->decision.state = AMS_STATE_SELF_TEST;
    } else if (controller->decision.state == AMS_STATE_SELF_TEST) {
        controller->decision.state = AMS_STATE_STANDBY;
    }

    if (controller->decision.latched_faults == AMS_FAULT_NONE &&
        AMS_HARDWARE_OUTPUTS_COMMISSIONED != 0U) {
        controller->decision.shutdown_closed_request = 1U;
    }
}
