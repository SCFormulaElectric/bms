#include "Tasks/CAN/bms_config_task.h"

#include <string.h>

#include "Ams/ams_persistence.h"
#include "Peripherals/bms_can_protocol.h"
#include "Peripherals/bms_summary_config.h"

typedef struct {
    bms_summary_config_t staging;
    uint8_t transaction_active;
    uint8_t transaction_id;
} bms_config_service_t;

static uint32_t get_u32(const uint8_t *source)
{
    return (uint32_t)source[0] | ((uint32_t)source[1] << 8U) |
        ((uint32_t)source[2] << 16U) | ((uint32_t)source[3] << 24U);
}

static void put_u16(uint8_t *destination, uint16_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
}

static void put_u32(uint8_t *destination, uint32_t value)
{
    destination[0] = (uint8_t)value;
    destination[1] = (uint8_t)(value >> 8U);
    destination[2] = (uint8_t)(value >> 16U);
    destination[3] = (uint8_t)(value >> 24U);
}

static void send_ack(app_data_t *data, uint8_t opcode, uint8_t transaction_id,
    bms_can_status_t status)
{
    can_tx_message_t response = {0};
    bms_summary_config_t active;

    taskENTER_CRITICAL();
    memcpy(&active, &data->summary_config, sizeof(active));
    taskEXIT_CRITICAL();
    response.id = BMS_CAN_CONFIG_RESPONSE_ID;
    response.dlc = BMS_CAN_FRAME_DLC;
    response.data[0] = BMS_CAN_PROTOCOL_VERSION;
    response.data[1] = opcode;
    response.data[2] = transaction_id;
    response.data[3] = (uint8_t)status;
    response.data[4] = active.generation;
    response.data[5] = active.active_count;
    put_u16(&response.data[6], (uint16_t)(active.period_ms / 100U));
    (void)can_bus_queue_message(&data->can_bus, &response);
}

static void send_slot(app_data_t *data, uint8_t transaction_id,
    uint8_t slot_index)
{
    can_tx_message_t response = {0};
    bms_summary_config_t active;

    taskENTER_CRITICAL();
    memcpy(&active, &data->summary_config, sizeof(active));
    taskEXIT_CRITICAL();
    response.id = BMS_CAN_LAYOUT_RESPONSE_BASE_ID + slot_index;
    response.dlc = BMS_CAN_FRAME_DLC;
    response.data[0] = BMS_CAN_PROTOCOL_VERSION;
    response.data[1] = transaction_id;
    response.data[2] = active.generation;
    response.data[3] = slot_index;
    put_u32(&response.data[4],
        bms_summary_pack_descriptor(&active.slots[slot_index]));
    (void)can_bus_queue_message(&data->can_bus, &response);
}

static uint8_t transaction_matches(const bms_config_service_t *service,
    uint8_t transaction_id)
{
    return (service->transaction_active != 0U &&
        service->transaction_id == transaction_id) ? 1U : 0U;
}

static void handle_command(app_data_t *data, bms_config_service_t *service,
    const can_rx_message_t *message)
{
    const uint8_t opcode = message->data[1];
    const uint8_t transaction_id = message->data[2];
    bms_can_status_t result = BMS_CAN_STATUS_OK;

    if (message->data[0] != BMS_CAN_PROTOCOL_VERSION) {
        send_ack(data, opcode, transaction_id, BMS_CAN_STATUS_BAD_VERSION);
        return;
    }
    switch (opcode) {
        case BMS_CAN_CMD_READ_CONFIG:
            break;
        case BMS_CAN_CMD_READ_SLOT:
            if (message->data[3] >= BMS_CAN_SUMMARY_MAX_FRAMES) {
                result = BMS_CAN_STATUS_BAD_VALUE;
            } else {
                send_slot(data, transaction_id, message->data[3]);
                return;
            }
            break;
        case BMS_CAN_CMD_CLEAR_FAULTS:
            result = bms_can_validate_clear_faults_request(message->data);
            if (result == BMS_CAN_STATUS_OK) {
                taskENTER_CRITICAL();
                if (data->fault_clear_requested != 0U) {
                    result = BMS_CAN_STATUS_BUSY;
                } else {
                    data->fault_clear_requested = 1U;
                }
                taskEXIT_CRITICAL();
            }
            break;
        case BMS_CAN_CMD_BEGIN:
            taskENTER_CRITICAL();
            memcpy(&service->staging, &data->summary_config,
                sizeof(service->staging));
            taskEXIT_CRITICAL();
            service->transaction_active = 1U;
            service->transaction_id = transaction_id;
            break;
        case BMS_CAN_CMD_SET_COUNT:
            if (service->transaction_active == 0U) {
                result = BMS_CAN_STATUS_NO_TRANSACTION;
            } else if (transaction_matches(service, transaction_id) == 0U) {
                result = BMS_CAN_STATUS_BAD_TRANSACTION;
            } else if (message->data[3] > BMS_CAN_SUMMARY_MAX_FRAMES) {
                result = BMS_CAN_STATUS_BAD_VALUE;
            } else {
                service->staging.active_count = message->data[3];
            }
            break;
        case BMS_CAN_CMD_SET_SLOT:
            if (service->transaction_active == 0U) {
                result = BMS_CAN_STATUS_NO_TRANSACTION;
            } else if (transaction_matches(service, transaction_id) == 0U) {
                result = BMS_CAN_STATUS_BAD_TRANSACTION;
            } else if (message->data[3] >= BMS_CAN_SUMMARY_MAX_FRAMES) {
                result = BMS_CAN_STATUS_BAD_VALUE;
            } else {
                bms_summary_unpack_descriptor(get_u32(&message->data[4]),
                    &service->staging.slots[message->data[3]]);
            }
            break;
        case BMS_CAN_CMD_VALIDATE:
            if (service->transaction_active == 0U) {
                result = BMS_CAN_STATUS_NO_TRANSACTION;
            } else if (transaction_matches(service, transaction_id) == 0U) {
                result = BMS_CAN_STATUS_BAD_TRANSACTION;
            } else if (bms_summary_config_is_valid(&service->staging) == 0U) {
                result = BMS_CAN_STATUS_BAD_VALUE;
            }
            break;
        case BMS_CAN_CMD_COMMIT:
            if (service->transaction_active == 0U) {
                result = BMS_CAN_STATUS_NO_TRANSACTION;
            } else if (transaction_matches(service, transaction_id) == 0U) {
                result = BMS_CAN_STATUS_BAD_TRANSACTION;
            } else if (bms_summary_config_is_valid(&service->staging) == 0U) {
                result = BMS_CAN_STATUS_BAD_VALUE;
            } else {
                uint8_t next_generation;
                taskENTER_CRITICAL();
                next_generation = (uint8_t)(data->summary_config.generation + 1U);
                if (next_generation == 0U) {
                    next_generation = 1U;
                }
                service->staging.generation = next_generation;
                memcpy(&data->summary_config, &service->staging,
                    sizeof(data->summary_config));
                taskEXIT_CRITICAL();
                service->transaction_active = 0U;
            }
            break;
        case BMS_CAN_CMD_SAVE:
        {
            bms_summary_config_t active;
            ams_persistence_status_t persistence;
            taskENTER_CRITICAL();
            memcpy(&active, &data->summary_config, sizeof(active));
            taskEXIT_CRITICAL();
            persistence = ams_persistence_save(&data->config, &active,
                data->soc_permille, data->discharge_enable_request,
                data->charge_enable_request);
            if (persistence == AMS_PERSISTENCE_OUTPUTS_ACTIVE) {
                result = BMS_CAN_STATUS_PERSISTENCE_BLOCKED;
            } else if (persistence != AMS_PERSISTENCE_OK) {
                result = BMS_CAN_STATUS_PERSISTENCE_ERROR;
            }
            break;
        }
        case BMS_CAN_CMD_ABORT:
            if (service->transaction_active == 0U) {
                result = BMS_CAN_STATUS_NO_TRANSACTION;
            } else if (transaction_matches(service, transaction_id) == 0U) {
                result = BMS_CAN_STATUS_BAD_TRANSACTION;
            } else {
                service->transaction_active = 0U;
            }
            break;
        default:
            result = BMS_CAN_STATUS_BAD_OPCODE;
            break;
    }
    send_ack(data, opcode, transaction_id, result);
}

HAL_StatusTypeDef bms_can_configure_rx_filter(CAN_HandleTypeDef *hcan)
{
    CAN_FilterTypeDef filter = {0};

    if (hcan == NULL) {
        return HAL_ERROR;
    }
    filter.FilterBank = 0U;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (uint16_t)(BMS_CAN_COMMAND_ID << 5U);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = (uint16_t)(0x7FFU << 5U);
    /* Compare IDE and RTR against zero: standard data frames only. */
    filter.FilterMaskIdLow = 0x0006U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14U;
    return HAL_CAN_ConfigFilter(hcan, &filter);
}

void bms_config_task(void *argument)
{
    app_data_t *data = (app_data_t *)argument;
    bms_config_service_t service = {0};
    can_rx_message_t message;

    for (;;) {
        if (xQueueReceive(data->can_bus.can_rx_queue, &message,
            pdMS_TO_TICKS(BMS_CONFIG_RX_WAIT_MS)) == pdPASS) {
            handle_command(data, &service, &message);
        }
    }
}

task_entry_t create_bms_config_task(app_data_t *data)
{
    task_entry_t entry = {0};
    const BaseType_t status = xTaskCreate(bms_config_task,
        "BMS config", BMS_CONFIG_STACK_SIZE, data, BMS_CONFIG_PRIO,
        &entry.handle);

    configASSERT(status == pdPASS);
    data->can_bus.rx_task_handle = entry.handle;
    vTaskSuspend(entry.handle);
    entry.name = "bms_config";
    return entry;
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef header = {0};
    can_rx_message_t message = {0};
    BaseType_t task_woken = pdFALSE;

    if (hcan != &hcan1 || HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0,
        &header, message.data) != HAL_OK) {
        app.can_bus.rx_malformed++;
        return;
    }
    message.id = (header.IDE == CAN_ID_STD) ? header.StdId : header.ExtId;
    message.dlc = (uint8_t)header.DLC;
    if (header.IDE != CAN_ID_STD || header.RTR != CAN_RTR_DATA ||
        message.id != BMS_CAN_COMMAND_ID) {
        app.can_bus.rx_rejected++;
    } else if (message.dlc != BMS_CAN_FRAME_DLC) {
        app.can_bus.rx_malformed++;
    } else if (app.can_bus.can_rx_queue == NULL ||
        xQueueSendFromISR(app.can_bus.can_rx_queue, &message,
            &task_woken) != pdPASS) {
        app.can_bus.rx_dropped++;
    } else if (app.can_bus.rx_task_handle != NULL) {
        vTaskNotifyGiveFromISR(app.can_bus.rx_task_handle, &task_woken);
    }
    portYIELD_FROM_ISR(task_woken);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan != &hcan1) {
        return;
    }
    if ((HAL_CAN_GetError(hcan) & HAL_CAN_ERROR_BOF) != 0U) {
        app.can_bus.bus_off_count++;
        app.can_bus.tx_fault_latched = 1U;
    }
}
