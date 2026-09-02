#ifndef CAN_BUS_H
#define CAN_BUS_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "queue.h"
#include "stm32f4xx_hal.h"
#include "task.h"

typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
} can_tx_message_t;

typedef struct {
    CAN_HandleTypeDef *hcan;
    QueueHandle_t can_tx_queue;
    TaskHandle_t tx_task_handle;
    volatile uint32_t tx_errors;
    volatile uint32_t bus_off_count;
    volatile uint8_t tx_fault_latched;
} can_bus_t;

BaseType_t can_bus_queue_message(can_bus_t *can_bus,
    const can_tx_message_t *message);

#endif /* CAN_BUS_H */
