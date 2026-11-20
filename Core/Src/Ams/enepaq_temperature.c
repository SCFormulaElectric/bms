#include "Ams/enepaq_temperature.h"

#include <stddef.h>

typedef struct {
    uint16_t mv;    /* sensor voltage, millivolts   */
    int16_t  dc;    /* temperature, deci-degrees C  */
} voltTempPoint;

/* Enepaq datasheet rev D, Table 5. Voltage falls as temperature rises. */
static const voltTempPoint voltTempTable[] = {
    { 2440, -400 }, { 2420, -350 }, { 2400, -300 }, { 2380, -250 },
    { 2350, -200 }, { 2320, -150 }, { 2270, -100 }, { 2230,  -50 },
    { 2170,    0 }, { 2110,   50 }, { 2050,  100 }, { 1990,  150 },
    { 1920,  200 }, { 1860,  250 }, { 1800,  300 }, { 1740,  350 },
    { 1680,  400 }, { 1630,  450 }, { 1590,  500 }, { 1550,  550 },
    { 1510,  600 }, { 1480,  650 }, { 1450,  700 }, { 1430,  750 },
    { 1400,  800 }, { 1380,  850 }, { 1370,  900 }, { 1350,  950 },
    { 1340, 1000 }, { 1330, 1050 }, { 1320, 1100 }, { 1310, 1150 },
    { 1300, 1200 }
};

#define TABLE_POINTS (sizeof(voltTempTable) / sizeof(voltTempTable[0]))

voltageStatus volToTemp(int32_t voltage, int16_t *temperature)
{
    size_t index;

    if (temperature == NULL) {
        return invalidArgument;
    }
    /* Outside the table: never clamp. An open wire reads high and would
     * otherwise look like a cold, safe cell. */
    if (voltage > voltTempTable[0].mv) {
        return aboveRange;
    }
    if (voltage < voltTempTable[TABLE_POINTS - 1U].mv) {
        return belowRange;
    }

    /* Voltages fall row by row: stop at the first row whose next
     * voltage is at or below the input. */
    for (index = 0U; index < TABLE_POINTS - 1U; index++) {
        const voltTempPoint *colder = &voltTempTable[index];
        const voltTempPoint *hotter = &voltTempTable[index + 1U];

        if (voltage >= hotter->mv) {
            const int32_t spanMv = (int32_t)colder->mv - hotter->mv;
            const int32_t spanDc = (int32_t)hotter->dc - colder->dc;
            const int32_t offsetMv = (int32_t)colder->mv - voltage;

            /* Linear interpolation, rounded to the nearest 0.1 C. */
            *temperature = (int16_t)(colder->dc +
                (offsetMv * spanDc + spanMv / 2) / spanMv);
            return OKRange;
        }
    }
    return belowRange;   /* not reachable after the range checks */
}
