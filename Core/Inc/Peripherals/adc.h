#ifndef ADC_H
#define ADC_H
#include <stdint.h>
#include "Peripherals/adc_snapshot.h"

#define ADC_CHANNEL_COUNT       ADC_SNAPSHOT_CHANNEL_COUNT
#define ADC_DMA_SCAN_COUNT      2U
#define ADC_DMA_BUFFER_COUNT    (ADC_CHANNEL_COUNT * ADC_DMA_SCAN_COUNT)
#define ADC_TRIGGER_RATE_HZ     1000U

/* Local ADC channels are reserved for board-level analog measurements. Cell
 * voltages and distributed temperatures must come through the AFE interface. */
#define ADC_SNAPSHOT_MAX_AGE_MS 50U

/* ADC1 channels are PC0..PC5, in this order. */

extern volatile uint16_t adc_dma_buffer[ADC_DMA_BUFFER_COUNT];

void adc_acquisition_init(void);
adc_snapshot_status_t adc_acquisition_read(adc_snapshot_t *snapshot,
    uint32_t now_ms);
#endif
