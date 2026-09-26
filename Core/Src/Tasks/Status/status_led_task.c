#include "Tasks/Status/status_led_task.h"

#include "Peripherals/digital_pins.h"

/* Sole owner of the PB0 status LED. The blink shows the scheduler is alive;
 * it is not part of the watchdog-proven safety path. */
void status_led_task(void *argument)
{
    TickType_t last_wake = xTaskGetTickCount();

    (void)argument;
    for (;;) {
        HAL_GPIO_TogglePin(AMS_STATUS_LED_GPIO_PORT, AMS_STATUS_LED_PIN);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(STATUS_LED_HALF_PERIOD_MS));
    }
}

task_entry_t create_status_led_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(status_led_task, "Status LED",
        STATUS_LED_STACK_SIZE, data, STATUS_LED_PRIO, &entry.handle);

    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);
    entry.name = "status_led";
    return entry;
}
