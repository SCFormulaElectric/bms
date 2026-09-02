#ifndef WATCHDOG_TASKS_DEFS_H
#define WATCHDOG_TASKS_DEFS_H

#define WD_AMS_CYCLE (1UL << 0)
#define WD_CAN_TX    (1UL << 1)
#define WD_IDWG      (1UL << 2)
#define WD_SD_CARD   (1UL << 3)

/* The AMS critical cycle includes acquisition, validation, fault evaluation,
 * and the shutdown-output update. Optional communications and logging are not
 * allowed to prevent the safety loop from completing. */
#define WD_REQUIRED_TASKS WD_AMS_CYCLE

#endif /* WATCHDOG_TASKS_DEFS_H */
