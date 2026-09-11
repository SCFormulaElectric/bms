#ifndef BMS_SUMMARY_TASK_H
#define BMS_SUMMARY_TASK_H

#include "app.h"

#define BMS_SUMMARY_STACK_SIZE       KILOBYTE
void bms_summary_task(void *argument);
task_entry_t create_bms_summary_task(app_data_t *data);

#endif /* BMS_SUMMARY_TASK_H */
