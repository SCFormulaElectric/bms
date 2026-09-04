#include <assert.h>

#include "Ams/ams_config.h"

int main(void)
{
    ams_config_t config;

    ams_config_load_defaults(&config);
    assert(ams_config_is_valid(&config) != 0U);
    assert(config.periodic_can_base_id == AMS_PERIODIC_CAN_DEFAULT_ID);
    assert(config.periodic_can_frame_count ==
        AMS_PERIODIC_CAN_DEFAULT_FRAMES);

    config.periodic_can_frame_count = 0U;
    assert(ams_config_is_valid(&config) == 0U);
    config.periodic_can_frame_count = 3U;
    assert(ams_config_is_valid(&config) == 0U);
    config.periodic_can_frame_count = 1U;
    config.periodic_can_base_id = 0x7FFU;
    assert(ams_config_is_valid(&config) != 0U);
    config.periodic_can_frame_count = 2U;
    assert(ams_config_is_valid(&config) == 0U);
    config.periodic_can_base_id = 0x800U;
    config.periodic_can_frame_count = 1U;
    assert(ams_config_is_valid(&config) == 0U);
    return 0;
}
