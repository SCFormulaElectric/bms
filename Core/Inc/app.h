#ifndef APP_H
#define APP_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "Ams/ams_types.h"
#include "FreeRTOS.h"
#include "main.h"
#include "Peripherals/can_bus.h"
#include "event_groups.h"
#include "queue.h"
#include "task.h"

#define NUM_TASKS              3U
#define AMS_CYCLE_PRIO         16U
#define IWDG_PRIO              16U
#define CAN_PRIO               10U

#define CAN_QUEUE_LENGTH       16U
#define LOG_MSG_MAX_LEN        128U
#define LOG_QUEUE_LENGTH       64U
#define KILOBYTE               256U
#define AMS_FIRMWARE_VERSION   "ams-f405-custom-bms-0.3.0"
#define AMS_TARGET_MCU         "STM32F405RGT6"

typedef enum {
    LOG_SERIAL,
    LOG_NONE
} log_level_t;

typedef struct {
    const char *name;
    TaskHandle_t handle;
} task_entry_t;

typedef struct app_data_s {
    task_entry_t task_entries[NUM_TASKS];
    log_level_t log_level;
    can_bus_t can_bus;
    EventGroupHandle_t watchdog_events;
    volatile ams_state_t ams_state;
    volatile uint32_t active_faults;
    volatile uint32_t latched_faults;
    volatile uint8_t discharge_enable_request;
    volatile uint8_t charge_enable_request;
    volatile uint8_t fan_enable_request;
    volatile uint16_t soc_permille;
    volatile uint8_t boot_healthy;
    ams_config_t config;
    uint16_t initial_soc_permille;
    volatile uint8_t afe_bringup_status;
    volatile uint8_t afe_bringup_failed_step;
    volatile uint8_t afe_verified_devices;
    volatile uint8_t afe_bridge_device_config;
    ams_fault_record_t first_fault;
} app_data_t;

extern app_data_t app;
void create_app(void);
void serial_log(const char *fmt, ...);

#endif /* APP_H */
