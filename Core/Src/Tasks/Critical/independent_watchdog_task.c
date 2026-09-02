#include "Tasks/Critical/independent_watchdog_task.h"

// Task: Independent Watchdog

void independent_watchdog_task(void *argument) {
    app_data_t *data = (app_data_t *) argument;
    const TickType_t window = pdMS_TO_TICKS(IDWG_WINDOW_MS);
    EventBits_t bits;
    for (;;)
    {
        xEventGroupSetBits(data->watchdog_events, WD_IDWG);
        bits = xEventGroupWaitBits(data->watchdog_events, WD_REQUIRED_TASKS,
            pdTRUE, pdTRUE, window);

        if ((bits & WD_REQUIRED_TASKS) == WD_REQUIRED_TASKS)
        {
            HAL_IWDG_Refresh(&hiwdg);
        }
        else 
        {
            EventBits_t missing = WD_REQUIRED_TASKS & (~bits);
            for (size_t i = 0; i < NUM_TASKS; i++) 
            {
                if (missing & (1 << i)) 
                {
                    serial_log("IWDG missing heartbeat bit %lu\r\n",
                        (unsigned long)i);
                }
            }
            for (;;) {
                // Wait for IWDG to reset the MCU
            }
        }
    }
}

task_entry_t create_independent_watchdog_task(app_data_t *data) {
    task_entry_t entry = {0};

    BaseType_t status = xTaskCreate(
        independent_watchdog_task,
        "Independent Watchdog",               // Task name (string)
        IDWG_STACK_SIZE,                     // Stack size (words, adjust as needed)
        data,                    // Task parameters
        IWDG_PRIO,
        &entry.handle
    );
    configASSERT(status == pdPASS);
    vTaskSuspend(entry.handle);

    entry.name = "idwg";
    return entry;
}
