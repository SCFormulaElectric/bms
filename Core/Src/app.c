#include "app.h"

#include "Tasks/Task_Helper/handles.h"
#include "Tasks/Task_Helper/tasks.h"

app_data_t app = {0};

static StaticQueue_t can_tx_queue_storage;
static uint8_t can_tx_items[CAN_QUEUE_LENGTH * sizeof(can_tx_message_t)];
static StaticQueue_t log_queue_storage;
static uint8_t log_items[LOG_QUEUE_LENGTH * sizeof(log_msg_t)];
static QueueHandle_t log_queue;
static EventGroupHandle_t watchdog_events;

void create_app(void)
{
    uint32_t index;

    app.log_level = LOG_NONE;
    app.ams_state = AMS_STATE_INIT;
    app.boot_healthy = 0U;
    app.can_bus.hcan = &hcan1;
    app.can_bus.can_tx_queue = xQueueCreateStatic(CAN_QUEUE_LENGTH,
        sizeof(can_tx_message_t), can_tx_items, &can_tx_queue_storage);
    configASSERT(app.can_bus.can_tx_queue != NULL);

    log_queue = xQueueCreateStatic(LOG_QUEUE_LENGTH, sizeof(log_msg_t),
        log_items, &log_queue_storage);
    configASSERT(log_queue != NULL);
    app.sd_card.sd_card_q = log_queue;
    storage_policy_initialize(&app.sd_card.policy, 0U);

    watchdog_events = xEventGroupCreate();
    configASSERT(watchdog_events != NULL);
    app.watchdog_events = watchdog_events;

    app.task_entries[ams_cycle_task_index] = create_ams_cycle_task(&app);
    app.task_entries[can_transmitter_task_index] =
        create_can_transmitter_task(&app);
    app.task_entries[independent_watchdog_task_index] =
        create_independent_watchdog_task(&app);
    app.task_entries[sd_card_task_index] = create_sd_card_task(&app);

    for (index = 0U; index < NUM_TASKS; index++) {
        if (index != sd_card_task_index) {
            configASSERT(app.task_entries[index].handle != NULL);
        }
    }
    for (index = 0U; index < NUM_TASKS; index++) {
        if (index != independent_watchdog_task_index &&
            app.task_entries[index].handle != NULL) {
            vTaskResume(app.task_entries[index].handle);
        }
    }
#if AMS_IWDG_ENABLE
    vTaskResume(app.task_entries[independent_watchdog_task_index].handle);
#endif
}

void serial_log(const char *fmt, ...)
{
    log_msg_t log;
    va_list args;
    int written;

    if (app.log_level == LOG_NONE) {
        return;
    }
    written = snprintf(log.line, sizeof(log.line), "[%lu] ",
        (unsigned long)HAL_GetTick());
    if (written < 0 || (size_t)written >= sizeof(log.line)) {
        return;
    }
    va_start(args, fmt);
    (void)vsnprintf(&log.line[written], sizeof(log.line) - (size_t)written,
        fmt, args);
    va_end(args);

    if (app.log_level == LOG_SERIAL) {
        (void)CDC_Transmit_FS((uint8_t *)log.line,
            (uint16_t)strnlen(log.line, sizeof(log.line)));
    } else if (xQueueSend(log_queue, &log, 0U) != pdPASS) {
        app.sd_card.policy.dropped_records++;
    }
}
