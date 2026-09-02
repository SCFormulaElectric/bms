#ifndef AMS_CONFIG_H
#define AMS_CONFIG_H

#include <stdint.h>

/* Placeholder topology. These values must be replaced from the accumulator
 * design before AMS_TOPOLOGY_COMMISSIONED may be set to one. */
#define AMS_SEGMENT_COUNT                 8U
#define AMS_CELLS_PER_SEGMENT            18U
#define AMS_TEMPS_PER_SEGMENT            16U
#define AMS_MAX_CELLS                     (AMS_SEGMENT_COUNT * AMS_CELLS_PER_SEGMENT)
#define AMS_MAX_TEMPERATURES              (AMS_SEGMENT_COUNT * AMS_TEMPS_PER_SEGMENT)

/* Conservative development limits. Final values must come from the selected
 * cell data sheet, thermal characterization, and the governing rule set. */
#define AMS_CELL_OVERVOLTAGE_MV           4200
#define AMS_CELL_UNDERVOLTAGE_MV          2800
#define AMS_CELL_OVERCURRENT_MA           300000
#define AMS_CELL_MAX_TEMPERATURE_DC       600
#define AMS_CELL_MIN_TEMPERATURE_DC       0

/* Formula Student 2026 baseline persistence windows. */
#define AMS_VOLTAGE_CURRENT_PERSIST_MS     500U
#define AMS_TEMPERATURE_PERSIST_MS         1000U
#define AMS_CRITICAL_CYCLE_PERIOD_MS       20U

/* AFE transport bounds. The final device profile may use less space but may
 * never exceed this fixed DMA allocation. */
#define AMS_AFE_DMA_BUFFER_BYTES            256U
#define AMS_AFE_DMA_TIMEOUT_MS              5U
#define AMS_AFE_SERVICE_POLL_MS              1U

/* Both gates intentionally default to zero. No software path may close the
 * shutdown circuit until the topology, AFE, pin map, and hardware SDC power
 * stage have been reviewed and commissioned. */
#define AMS_TOPOLOGY_COMMISSIONED          0U
#define AMS_HARDWARE_OUTPUTS_COMMISSIONED  0U

#endif /* AMS_CONFIG_H */
