#ifndef UART_BROADCAST_TASK_H
#define UART_BROADCAST_TASK_H

#include "app.h"

#define UART_BROADCAST_STACK_SIZE    (2U * KILOBYTE)
#define UART_BROADCAST_PERIOD_MS     1000U
/* A full 8 x 12 cell report is about 2.3 KB, roughly 200 ms at 115200. */
#define UART_BROADCAST_BUFFER_BYTES  3072U
#define UART_BROADCAST_TX_TIMEOUT_MS 1000U

/* Periodically prints AFE bring-up/runtime status, AMS state and every
 * valid cell and GPIO voltage on USART1 (J5, 115200 8N1) as key=value lines.
 * cell[n] is 1-based across the pack: segment s, cell c is
 * n = (s - 1) * cells_per_segment + c. Diagnostic only; it never blocks the
 * critical task and is not part of the watchdog-proven path. */
void uart_broadcast_task(void *argument);
task_entry_t create_uart_broadcast_task(app_data_t *data);

#endif /* UART_BROADCAST_TASK_H */
