#include "app.h"

#include "main.h"
#include "Tasks/Task_Helper/handles.h"
#include "Tasks/Task_Helper/tasks.h"

app_data_t app = {0};

static StaticQueue_t can_tx_queue_storage;
static uint8_t can_tx_items[CAN_QUEUE_LENGTH * sizeof(can_tx_message_t)];
static StaticQueue_t can_rx_queue_storage;
static uint8_t can_rx_items[CAN_RX_QUEUE_LENGTH * sizeof(can_rx_message_t)];
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
    app.can_bus.can_rx_queue = xQueueCreateStatic(CAN_RX_QUEUE_LENGTH,
        sizeof(can_rx_message_t), can_rx_items, &can_rx_queue_storage);
    configASSERT(app.can_bus.can_rx_queue != NULL);

    watchdog_events = xEventGroupCreate();
    configASSERT(watchdog_events != NULL);
    app.watchdog_events = watchdog_events;

    app.task_entries[ams_cycle_task_index] = create_ams_cycle_task(&app);
    app.task_entries[can_transmitter_task_index] =
        create_can_transmitter_task(&app);
    app.task_entries[bms_config_task_index] = create_bms_config_task(&app);
    app.task_entries[bms_summary_task_index] = create_bms_summary_task(&app);
    app.task_entries[independent_watchdog_task_index] =
        create_independent_watchdog_task(&app);

    for (index = 0U; index < NUM_TASKS; index++) {
        configASSERT(app.task_entries[index].handle != NULL);
    }
    for (index = 0U; index < NUM_TASKS; index++) {
        if (index != independent_watchdog_task_index) {
            vTaskResume(app.task_entries[index].handle);
        }
    }
#if AMS_IWDG_ENABLE
    vTaskResume(app.task_entries[independent_watchdog_task_index].handle);
#endif
}

void serial_log(const char *fmt, ...)
{
    /* The board exposes USART1 on J5. Logging remains disabled until a
     * nonblocking UART queue is commissioned; never block the safety loop. */
    (void)fmt;
}
