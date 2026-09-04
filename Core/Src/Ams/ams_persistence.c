#include "Ams/ams_persistence.h"

#include <stddef.h>
#include <string.h>

#include "stm32f4xx_hal.h"

#define AMS_FLASH_SECTOR_ADDRESS  0x080E0000UL
#define AMS_FLASH_SECTOR_BYTES    (128UL * 1024UL)
#define AMS_FLASH_RECORD_MAGIC    0x414D5343UL
#define AMS_FLASH_RECORD_VERSION  1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t record_bytes;
    uint32_t sequence;
    uint16_t soc_permille;
    uint16_t reserved;
    ams_config_t config;
    uint32_t crc32;
} ams_flash_record_t;

static uint32_t record_stride(void)
{
    return (uint32_t)((sizeof(ams_flash_record_t) + 3U) & ~3U);
}

static uint32_t crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t index;

    for (index = 0U; index < length; index++) {
        uint32_t bit;
        crc ^= data[index];
        for (bit = 0U; bit < 8U; bit++) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0U);
        }
    }
    return ~crc;
}

static uint8_t record_valid(const ams_flash_record_t *record)
{
    if (record->magic != AMS_FLASH_RECORD_MAGIC ||
        record->version != AMS_FLASH_RECORD_VERSION ||
        record->record_bytes != sizeof(*record) ||
        record->soc_permille > 1000U ||
        ams_config_is_valid(&record->config) == 0U) {
        return 0U;
    }
    return (crc32((const uint8_t *)record,
        offsetof(ams_flash_record_t, crc32)) == record->crc32) ? 1U : 0U;
}

static const ams_flash_record_t *latest_record(uint32_t *next_address)
{
    const ams_flash_record_t *latest = NULL;
    uint32_t address;
    const uint32_t stride = record_stride();
    const uint32_t end = AMS_FLASH_SECTOR_ADDRESS + AMS_FLASH_SECTOR_BYTES;

    for (address = AMS_FLASH_SECTOR_ADDRESS;
        address + sizeof(ams_flash_record_t) <= end; address += stride) {
        const ams_flash_record_t *candidate =
            (const ams_flash_record_t *)address;
        if (candidate->magic == 0xFFFFFFFFUL) {
            break;
        }
        if (record_valid(candidate) != 0U &&
            (latest == NULL ||
             (int32_t)(candidate->sequence - latest->sequence) > 0)) {
            latest = candidate;
        }
    }
    if (next_address != NULL) {
        *next_address = address;
    }
    return latest;
}

ams_persistence_status_t ams_persistence_load(ams_config_t *config,
    uint16_t *soc_permille, uint32_t *sequence)
{
    const ams_flash_record_t *record;

    if (config == NULL || soc_permille == NULL) {
        return AMS_PERSISTENCE_INVALID;
    }
    record = latest_record(NULL);
    if (record == NULL) {
        return AMS_PERSISTENCE_NOT_FOUND;
    }
    *config = record->config;
    *soc_permille = record->soc_permille;
    if (sequence != NULL) {
        *sequence = record->sequence;
    }
    return AMS_PERSISTENCE_OK;
}

ams_persistence_status_t ams_persistence_save(const ams_config_t *config,
    uint16_t soc_permille, uint8_t discharge_output_active,
    uint8_t charge_output_active)
{
    ams_flash_record_t record;
    const ams_flash_record_t *latest;
    uint32_t address;
    uint32_t offset;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t sector_error = 0U;
    HAL_StatusTypeDef status = HAL_OK;

    if (discharge_output_active != 0U || charge_output_active != 0U) {
        return AMS_PERSISTENCE_OUTPUTS_ACTIVE;
    }
    if (ams_config_is_valid(config) == 0U || soc_permille > 1000U) {
        return AMS_PERSISTENCE_INVALID;
    }
    latest = latest_record(&address);
    if (address + sizeof(record) >
        AMS_FLASH_SECTOR_ADDRESS + AMS_FLASH_SECTOR_BYTES) {
        address = AMS_FLASH_SECTOR_ADDRESS;
    }

    memset(&record, 0, sizeof(record));
    record.magic = AMS_FLASH_RECORD_MAGIC;
    record.version = AMS_FLASH_RECORD_VERSION;
    record.record_bytes = sizeof(record);
    record.sequence = (latest == NULL) ? 1U : latest->sequence + 1U;
    record.soc_permille = soc_permille;
    record.config = *config;
    record.crc32 = crc32((const uint8_t *)&record,
        offsetof(ams_flash_record_t, crc32));

    if (HAL_FLASH_Unlock() != HAL_OK) {
        return AMS_PERSISTENCE_FLASH_ERROR;
    }
    if (address == AMS_FLASH_SECTOR_ADDRESS &&
        *(const uint32_t *)address != 0xFFFFFFFFUL) {
        erase.TypeErase = FLASH_TYPEERASE_SECTORS;
        erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        erase.Sector = FLASH_SECTOR_11;
        erase.NbSectors = 1U;
        status = HAL_FLASHEx_Erase(&erase, &sector_error);
    }
    for (offset = 0U; status == HAL_OK && offset < sizeof(record);
        offset += sizeof(uint32_t)) {
        uint32_t word = 0xFFFFFFFFUL;
        const uint32_t remaining = (uint32_t)sizeof(record) - offset;
        memcpy(&word, ((const uint8_t *)&record) + offset,
            (remaining < sizeof(word)) ? remaining : sizeof(word));
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
            address + offset, word);
    }
    (void)HAL_FLASH_Lock();
    if (status != HAL_OK ||
        record_valid((const ams_flash_record_t *)address) == 0U) {
        return AMS_PERSISTENCE_FLASH_ERROR;
    }
    return AMS_PERSISTENCE_OK;
}
