#include "Tasks/Critical/ams_cycle_task.h"

#include <string.h>

#include "Ams/ams_afe.h"
#include "Ams/ams_controller.h"
#include "Peripherals/digital_pins.h"
#include "Tasks/Task_Helper/watchdog_tasks_defs.h"

static void apply_safety_outputs(const ams_decision_t *decision)
{
    GPIO_PinState shutdown_state = GPIO_PIN_RESET;

    if (decision != NULL &&
        AMS_HARDWARE_OUTPUTS_COMMISSIONED != 0U &&
        decision->shutdown_closed_request != 0U &&
        decision->latched_faults == AMS_FAULT_NONE) {
        shutdown_state = GPIO_PIN_SET;
    }
    HAL_GPIO_WritePin(AMS_SDC_ENABLE_GPIO_PORT, AMS_SDC_ENABLE_PIN,
        shutdown_state);
}

void ams_cycle_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    static ams_afe_t afe;
    static ams_controller_t controller;
    ams_afe_status_t afe_status;
    uint32_t sample_counter = 0U;

    ams_afe_reset(&afe);
    ams_controller_initialize(&controller);
    afe_status = ams_afe_initialize(&afe, &hspi1,
        ams_afe_default_profile());

    for (;;) {
        ams_measurement_t measurement;
        uint32_t immediate_faults = AMS_FAULT_NONE;
        uint8_t decision_ready = 0U;

        afe_status = ams_afe_service(&afe, (uint32_t)HAL_GetTick());
        if (afe_status == AMS_AFE_NEW_SAMPLE) {
            if (ams_afe_fetch_latest(&afe, &measurement,
                &sample_counter) == AMS_AFE_OK) {
                decision_ready = 1U;
            } else {
                afe_status = AMS_AFE_INVALID_DATA;
            }
        }
        if (afe_status != AMS_AFE_OK &&
            afe_status != AMS_AFE_IN_PROGRESS &&
            afe_status != AMS_AFE_NEW_SAMPLE) {
            memset(&measurement, 0, sizeof(measurement));
            measurement.timestamp_ms = (uint32_t)HAL_GetTick();
            measurement.status = AMS_SAMPLE_UNAVAILABLE;
            immediate_faults |= AMS_FAULT_AFE_COMMUNICATION;
            decision_ready = 1U;
        }

        if (decision_ready != 0U) {
            ams_controller_step(&controller, &measurement, immediate_faults,
                (uint32_t)HAL_GetTick());

            /* Output update is part of the critical-cycle proof. The watchdog
             * heartbeat is emitted only after this operation completes. */
            apply_safety_outputs(&controller.decision);
            taskENTER_CRITICAL();
            data->ams_state = controller.decision.state;
            data->active_faults = controller.decision.active_faults;
            data->latched_faults = controller.decision.latched_faults;
            data->shutdown_closed_request =
                controller.decision.shutdown_closed_request;
            data->first_fault = controller.decision.first_fault;
            taskEXIT_CRITICAL();
            xEventGroupSetBits(data->watchdog_events, WD_AMS_CYCLE);
        }

        vTaskDelay((afe_status == AMS_AFE_IN_PROGRESS) ?
            pdMS_TO_TICKS(AMS_AFE_SERVICE_POLL_MS) :
            pdMS_TO_TICKS(AMS_CRITICAL_CYCLE_PERIOD_MS));
    }
}

task_entry_t create_ams_cycle_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(ams_cycle_task, "AMS critical cycle",
        AMS_CYCLE_STACK_SIZE, data, AMS_CYCLE_PRIO, &entry.handle);
    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);
    entry.name = "ams_cycle";
    return entry;
}
