#include "Tasks/CAN/can_transmitter_task.h"

BaseType_t can_bus_queue_message(can_bus_t *can_bus,
    const can_tx_message_t *message)
{
    if (can_bus == NULL || message == NULL || message->dlc > 8U ||
        message->id > 0x7FFU || can_bus->can_tx_queue == NULL) {
        if (can_bus != NULL) {
            can_bus->tx_fault_latched = 1U;
        }
        return pdFAIL;
    }
    if (xQueueSend(can_bus->can_tx_queue, message, 0U) != pdPASS) {
        can_bus->tx_errors++;
        return pdFAIL;
    }
    if (can_bus->tx_task_handle != NULL) {
        xTaskNotifyGive(can_bus->tx_task_handle);
    }
    return pdPASS;
}

static void transmit(can_bus_t *can_bus, const can_tx_message_t *message)
{
    CAN_TxHeaderTypeDef header = {0};
    uint32_t mailbox = 0U;
    const TickType_t started = xTaskGetTickCount();

    while (HAL_CAN_GetTxMailboxesFreeLevel(can_bus->hcan) == 0U &&
        (xTaskGetTickCount() - started) <
            pdMS_TO_TICKS(CAN_TX_MAILBOX_WAIT_MS)) {
        vTaskDelay(pdMS_TO_TICKS(1U));
    }
    if (HAL_CAN_GetTxMailboxesFreeLevel(can_bus->hcan) == 0U) {
        can_bus->tx_errors++;
        can_bus->tx_fault_latched = 1U;
        return;
    }
    header.StdId = message->id;
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = message->dlc;
    if (HAL_CAN_AddTxMessage(can_bus->hcan, &header,
        (uint8_t *)message->data, &mailbox) != HAL_OK) {
        can_bus->tx_errors++;
        can_bus->tx_fault_latched = 1U;
    }
}

void can_transmitter_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    can_tx_message_t message;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE,
            pdMS_TO_TICKS(CAN_TX_FALLBACK_WAKE_MS));
        while (xQueueReceive(data->can_bus.can_tx_queue, &message, 0U) ==
            pdPASS) {
            transmit(&data->can_bus, &message);
        }
    }
}

task_entry_t create_can_transmitter_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(can_transmitter_task,
        "CAN transmitter", CAN_TX_STACK_SIZE, data, CAN_PRIO, &entry.handle);
    configASSERT(status == pdPASS);
    data->can_bus.tx_task_handle = entry.handle;
    vTaskSuspend(entry.handle);
    entry.name = "can_tx";
    return entry;
}
