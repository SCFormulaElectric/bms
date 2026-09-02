#ifndef CAN_TRANSMITTER_TASK_H
#define CAN_TRANSMITTER_TASK_H

#include "app.h"

#define CAN_TX_STACK_SIZE             (2U * KILOBYTE)
#define CAN_TX_MAILBOX_WAIT_MS        2U
#define CAN_TX_FALLBACK_WAKE_MS       20U

void can_transmitter_task(void *argument);
task_entry_t create_can_transmitter_task(app_data_t *data);

#endif /* CAN_TRANSMITTER_TASK_H */
