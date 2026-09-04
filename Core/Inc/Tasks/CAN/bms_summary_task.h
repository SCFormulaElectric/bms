#ifndef BMS_SUMMARY_TASK_H
#define BMS_SUMMARY_TASK_H

#include "app.h"

#define BMS_SUMMARY_STACK_SIZE       KILOBYTE
#define BMS_SUMMARY_PERIOD_MS        5000U
#define BMS_SUMMARY_FORMAT_VERSION      1U

void bms_summary_task(void *argument);
task_entry_t create_bms_summary_task(app_data_t *data);

#endif /* BMS_SUMMARY_TASK_H */
