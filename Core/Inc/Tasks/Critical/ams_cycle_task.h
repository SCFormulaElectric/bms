#ifndef AMS_CYCLE_TASK_H
#define AMS_CYCLE_TASK_H

#include "app.h"

#define AMS_CYCLE_STACK_SIZE (2U * KILOBYTE)

void ams_cycle_task(void *argument);
task_entry_t create_ams_cycle_task(app_data_t *data);

#endif /* AMS_CYCLE_TASK_H */
