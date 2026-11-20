#include <assert.h>
#include <stddef.h>

#include "Ams/enepaq_temperature.h"

/* Returns the converted temperature and asserts the conversion succeeded. */
static int16_t convert_ok(int32_t voltage)
{
    int16_t temperature = 0;

    assert(volToTemp(voltage, &temperature) == OKRange);
    return temperature;
}

static void test_exact_table_points(void)
{
    /* Datasheet Table 5 rows must come back exactly. */
    assert(convert_ok(2440) == -400);   /* first row: -40.0 C */
    assert(convert_ok(2170) == 0);      /*   0.0 C */
    assert(convert_ok(1920) == 200);    /*  20.0 C */
    assert(convert_ok(1860) == 250);    /*  25.0 C */
    assert(convert_ok(1510) == 600);    /*  60.0 C, discharge limit */
    assert(convert_ok(1300) == 1200);   /* last row: 120.0 C */
}

static void test_interpolation(void)
{
    /* 1900 mV: 20 mV into the 1920..1860 mV (20..25 C) span ->
     * 200 + 20 * 50 / 60 = 216.7 -> rounds to 217 (21.7 C). */
    assert(convert_ok(1900) == 217);
    /* 1455 mV: 25 mV into the 1480..1450 mV (65..70 C) span ->
     * 650 + 25 * 50 / 30 = 691.7 -> rounds to 692 (69.2 C). */
    assert(convert_ok(1455) == 692);
    /* 1305 mV: halfway between the last two rows (115..120 C). */
    assert(convert_ok(1305) == 1175);
}

static void test_out_of_range_is_rejected_not_clamped(void)
{
    int16_t temperature = 12345;   /* sentinel: must stay untouched */

    /* Open wire: pull-up drags the input high. */
    assert(volToTemp(2441, &temperature) == aboveRange);
    assert(volToTemp(5000, &temperature) == aboveRange);
    /* Shorted sensor or input near ground. */
    assert(volToTemp(1299, &temperature) == belowRange);
    assert(volToTemp(0, &temperature) == belowRange);
    assert(volToTemp(-100, &temperature) == belowRange);
    /* Invalid inputs must never write a temperature. */
    assert(temperature == 12345);
}

static void test_null_pointer(void)
{
    assert(volToTemp(1900, NULL) == invalidArgument);
}

static void test_whole_range_is_monotonic(void)
{
    /* Sweep every millivolt in the table: results stay within -40..120 C,
     * and a higher voltage never reads as a hotter temperature. */
    int32_t voltage;
    int16_t previous = convert_ok(1300);

    for (voltage = 1301; voltage <= 2440; voltage++) {
        const int16_t current = convert_ok(voltage);

        assert(current >= -400 && current <= 1200);
        assert(current <= previous);
        previous = current;
    }
}

int main(void)
{
    test_exact_table_points();
    test_interpolation();
    test_out_of_range_is_rejected_not_clamped();
    test_null_pointer();
    test_whole_range_is_monotonic();
    return 0;
}