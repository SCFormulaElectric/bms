#ifndef AMS_CYCLE_TASK_H
#define AMS_CYCLE_TASK_H

#include "app.h"

#define AMS_CYCLE_STACK_SIZE (2U * KILOBYTE)
/* DEBUG builds only: interval between retries of a failed AFE bring-up. */
#define AMS_AFE_BRINGUP_RETRY_MS  2000U

/* Blocking BQ79600/BQ79616 bring-up and measurement configuration for
 * data->config, then the bench balance test when AMS_BALANCE_BENCH_TEST_CELL
 * is set, followed on failure by the bridge diagnostics; publishes the
 * results to data. Call before the scheduler starts, or from the AMS task
 * while the AFE reader is idle (it busy-waits for tens of milliseconds). */
void ams_afe_bring_up(app_data_t *data);
void ams_cycle_task(void *argument);
task_entry_t create_ams_cycle_task(app_data_t *data);

#endif /* AMS_CYCLE_TASK_H */
