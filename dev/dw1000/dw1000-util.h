#ifndef DW1000_UTIL_H_
#define DW1000_UTIL_H_
#include "deca_device_api.h"
#include <stdbool.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/

/**
 * Estimate the transmission time of a frame in nanoseconds
 * dwt_config_t   dwt_config  Configuration struct of the DW1000
 * uint16_t       framelength Framelength including the 2-Byte CRC
 * bool           only_rmarker Option to compute only the time to the RMARKER (SHR time)
 *
 *
 * Source: https://tomlankhorst.nl/estimating-decawave-dw1000-tx-time/
 */
uint32_t dw1000_estimate_tx_time(const dwt_config_t* dwt_config, uint16_t framelength, bool only_rmarker);

/*---------------------------------------------------------------------------*/

#ifdef DW1000_CONF_CFO_PPM_PER_TRIM
#define DW1000_CFO_PPM_PER_TRIM (DW1000_CONF_CFO_PPM_PER_TRIM)
#else
#define DW1000_CFO_PPM_PER_TRIM (1.45)
#endif

#ifdef DW1000_CONF_CFO_JITTER_GUARD
#define DW1000_CFO_JITTER_GUARD (DW1000_CONF_CFO_JITTER_GUARD)
#else
#define DW1000_CFO_JITTER_GUARD (0.1)
#endif

#ifdef DW1000_CONF_CFO_WANTED
#define DW1000_CFO_WANTED (DW1000_CONF_CFO_WANTED)
#else
#define DW1000_CFO_WANTED (0) // desidered CFO is 0
#endif

void dw1000_set_ppm_per_trim(float ppm);
void dw1000_set_jitter_guard(float ppm);

/* Returns the clock frequency offset w.r.t. the sender in ppm for the given 
 * radio configuration, based on the carrier integrator. This function may be 
 * called after a frame reception. For further info, see the documentation for
 * dwt_readcarrierintegrator() of the DecaWave API. 
 *
 */
float dw1000_get_ppm_offset(const dwt_config_t *dwt_config);

/* Returns the XTAL trim value that best compensates the frequency offset
 *
 * curr_offset_ppm: current frequency offset
 * curr_trim: current trim values
 */
uint8_t dw1000_get_best_trim_code(float curr_offset_ppm, uint8_t curr_trim);

/*---------------------------------------------------------------------------*/

#endif //DW1000_UTIL_H_
