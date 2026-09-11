#ifndef BMS_CONFIG_TASK_H
#define BMS_CONFIG_TASK_H

#include "app.h"

#define BMS_CONFIG_STACK_SIZE       (2U * KILOBYTE)
#define BMS_CONFIG_RX_WAIT_MS                   100U

HAL_StatusTypeDef bms_can_configure_rx_filter(CAN_HandleTypeDef *hcan);
void bms_config_task(void *argument);
task_entry_t create_bms_config_task(app_data_t *data);

#endif /* BMS_CONFIG_TASK_H */
