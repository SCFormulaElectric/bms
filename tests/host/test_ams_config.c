#include <assert.h>

#include "Ams/ams_config.h"

int main(void)
{
    ams_config_t config;

    ams_config_load_defaults(&config);
    assert(ams_config_is_valid(&config) != 0U);

    config.segment_count = 0U;
    assert(ams_config_is_valid(&config) == 0U);
    return 0;
}
