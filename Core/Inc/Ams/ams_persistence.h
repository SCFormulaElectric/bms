#ifndef AMS_PERSISTENCE_H
#define AMS_PERSISTENCE_H

#include <stdint.h>
#include "Ams/ams_config.h"

typedef enum {
    AMS_PERSISTENCE_OK = 0,
    AMS_PERSISTENCE_NOT_FOUND,
    AMS_PERSISTENCE_INVALID,
    AMS_PERSISTENCE_FLASH_ERROR,
    AMS_PERSISTENCE_OUTPUTS_ACTIVE
} ams_persistence_status_t;

/* Sector 11 is reserved by the linker. Writes erase/program internal flash and
 * must only be requested while both charge and discharge outputs are off. */
ams_persistence_status_t ams_persistence_load(ams_config_t *config,
    uint16_t *soc_permille, uint32_t *sequence);
ams_persistence_status_t ams_persistence_save(const ams_config_t *config,
    uint16_t soc_permille, uint8_t discharge_output_active,
    uint8_t charge_output_active);

#endif /* AMS_PERSISTENCE_H */
