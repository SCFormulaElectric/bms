#pragma once
#include <stdint.h>

typedef enum {
    OKRange,
    belowRange,       /* voltage < 1300 mV: short, or hotter than 120 C   */
    aboveRange,       /* voltage > 2440 mV: open wire, or colder than -40 C */
    invalidArgument   /* temperature pointer was NULL */
} voltageStatus;

voltageStatus volToTemp(int32_t voltage, int16_t *temperature);