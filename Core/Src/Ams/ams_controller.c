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

void ams_controller_initialize(ams_controller_t *controller,
    const ams_config_t *config)
{
    if (controller == NULL) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    if (config != NULL) {
        controller->config = *config;
    }
    controller->decision.state = AMS_STATE_INIT;
}

void ams_controller_step(ams_controller_t *controller,
    const ams_measurement_t *measurement, uint32_t immediate_faults,
    uint32_t now_ms)
{
    uint32_t faults = immediate_faults;
    uint16_t index;
    uint16_t expected_cells;
    uint16_t expected_temperatures;
    uint8_t discharge_allowed = 1U;
    uint8_t charge_allowed = 1U;

    if (controller == NULL) {
        return;
    }
    controller->decision.discharge_enable_request = 0U;
    controller->decision.charge_enable_request = 0U;
    controller->decision.fan_enable_request = 0U;

    if (ams_config_is_valid(&controller->config) == 0U) {
        faults |= AMS_FAULT_CONFIGURATION | AMS_FAULT_NOT_COMMISSIONED;
    }
    expected_cells = ams_config_cell_count(&controller->config);
    expected_temperatures =
        ams_config_temperature_count(&controller->config);
    if (measurement == NULL || measurement->status != AMS_SAMPLE_VALID ||
        measurement->valid_cell_count != expected_cells ||
        measurement->valid_temperature_count != expected_temperatures ||
        (uint32_t)(now_ms - measurement->timestamp_ms) >
            controller->config.measurement_max_age_ms) {
        faults |= AMS_FAULT_AFE_COMMUNICATION;
    } else {
        int16_t maximum_temperature = measurement->temperature_dc[0];
        for (index = 0U; index < expected_cells; index++) {
            const int32_t voltage = measurement->cell_voltage_mv[index];
            const uint16_t segment =
                (uint16_t)(index / controller->config.cells_per_segment);
            const uint16_t channel =
                (uint16_t)(index % controller->config.cells_per_segment);
            if (voltage <= AMS_CELL_SOC_EMPTY_MV) {
                discharge_allowed = 0U;
            }
            if (voltage >= controller->config.cell_overvoltage_mv) {
                charge_allowed = 0U;
            }
            if (persistence_elapsed(&controller->voltage_high_active[index],
                &controller->voltage_high_since[index],
                (voltage > controller->config.cell_overvoltage_mv) ? 1U : 0U,
                now_ms, controller->config.voltage_current_persist_ms) != 0U) {
                faults |= AMS_FAULT_CELL_OVERVOLTAGE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_CELL_OVERVOLTAGE, now_ms, voltage,
                    controller->config.cell_overvoltage_mv, segment, channel);
            }
            if (persistence_elapsed(&controller->voltage_low_active[index],
                &controller->voltage_low_since[index],
                (voltage < controller->config.cell_undervoltage_mv) ? 1U : 0U,
                now_ms, controller->config.voltage_current_persist_ms) != 0U) {
                faults |= AMS_FAULT_CELL_UNDERVOLTAGE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_CELL_UNDERVOLTAGE, now_ms, voltage,
                    controller->config.cell_undervoltage_mv, segment, channel);
            }
        }
        for (index = 0U; index < expected_temperatures; index++) {
            const int32_t temperature = measurement->temperature_dc[index];
            const uint16_t segment = (uint16_t)(index /
                controller->config.temperatures_per_segment);
            const uint16_t channel = (uint16_t)(index %
                controller->config.temperatures_per_segment);
            if (temperature > maximum_temperature) {
                maximum_temperature = (int16_t)temperature;
            }
            if (persistence_elapsed(
                &controller->temperature_high_active[index],
                &controller->temperature_high_since[index],
                (temperature > controller->config.cell_max_temperature_dc) ?
                    1U : 0U, now_ms,
                controller->config.temperature_persist_ms) != 0U) {
                faults |= AMS_FAULT_OVERTEMPERATURE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_OVERTEMPERATURE, now_ms, temperature,
                    controller->config.cell_max_temperature_dc,
                    segment, channel);
            }
            if (persistence_elapsed(
                &controller->temperature_low_active[index],
                &controller->temperature_low_since[index],
                (temperature < controller->config.cell_min_temperature_dc) ?
                    1U : 0U, now_ms,
                controller->config.temperature_persist_ms) != 0U) {
                faults |= AMS_FAULT_UNDERTEMPERATURE;
                record_first_fault(&controller->decision,
                    AMS_FAULT_UNDERTEMPERATURE, now_ms, temperature,
                    controller->config.cell_min_temperature_dc,
                    segment, channel);
            }
        }
        if (maximum_temperature >=
            controller->config.fan_on_temperature_dc &&
            AMS_HARDWARE_OUTPUTS_COMMISSIONED != 0U) {
            controller->decision.fan_enable_request = 1U;
        }
        if (persistence_elapsed(&controller->discharge_overcurrent_active,
            &controller->discharge_overcurrent_since,
            (measurement->pack_current_ma >
                (int32_t)controller->config.discharge_current_limit_ma) ?
                1U : 0U, now_ms,
            controller->config.voltage_current_persist_ms) != 0U) {
            faults |= AMS_FAULT_DISCHARGE_OVERCURRENT;
            record_first_fault(&controller->decision,
                AMS_FAULT_DISCHARGE_OVERCURRENT, now_ms,
                measurement->pack_current_ma,
                (int32_t)controller->config.discharge_current_limit_ma,
                0U, 0U);
        }
        if (persistence_elapsed(&controller->charge_overcurrent_active,
            &controller->charge_overcurrent_since,
            (measurement->pack_current_ma <
                -(int32_t)controller->config.charge_current_limit_ma) ?
                1U : 0U, now_ms,
            controller->config.voltage_current_persist_ms) != 0U) {
            faults |= AMS_FAULT_CHARGE_OVERCURRENT;
            record_first_fault(&controller->decision,
                AMS_FAULT_CHARGE_OVERCURRENT, now_ms,
                measurement->pack_current_ma,
                -(int32_t)controller->config.charge_current_limit_ma,
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
        controller->decision.discharge_enable_request = discharge_allowed;
        controller->decision.charge_enable_request = charge_allowed;
    }
}
