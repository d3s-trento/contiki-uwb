#ifndef DW1000_UTIL_H_
#define DW1000_UTIL_H_
#include "deca_device_api.h"
#include <stdbool.h>
#include <stdint.h>

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
#endif //DW1000_UTIL_H_
