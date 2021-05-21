#ifndef DW1000_UTIL_H_
#define DW1000_UTIL_H_
#include "deca_device_api.h"
#include "contiki-conf.h"
#include "dw1000-cir.h"
#include <stdbool.h>
#include <stdint.h>

/*---------------------------------------------------------------------------*/

/* Structure for the RX power computation results */
typedef struct {
  uint32_t pac_correction; /* PAC normalization factor, affected by non-saturated PAC */
  double cir_pwr_norm; /* Adjusted CIR power from ((C * 2^17) / N^2)) where C is the raw CIR power and N is the PAC normalization factor */
  double fp_raw; /* First Path Power Level before SFD correction */
  double fp_pwr; /* First Path Power Level */
  double rx_pwr; /* RX Power Level */
} dw1000_rxpwr_t;

/* Structure for the NLOS analysis results */
typedef struct {
  double path_diff; /* Index difference between first and peak paths */
  double pr_nlos; /* NLOS probability based on path_diff */
  double low_noise; /* New noise threshold for early path detection */
  double num_early_peaks; /* Number of candidate early paths when low_noise is the threshold */
  double luep; /* Likelihood of undetected early path given num_early_peaks (can overrule pr_nlos) */
  double mc; /* Indicator for accumulator saturation in LOS conditions (can overrule pr_nlos) */
  double cl; /* Overall confidence level (0.0 -> NLOS, 1.0 -> LOS) */
} dw1000_nlos_t;

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

/* Change the jitter guard for best trim code computation.
 * The guard prevents the trim code from changing multiple times when
 * PPM offset is around half of the trim step.
 */
void dw1000_set_cfo_jitter_guard(double ppm);

/* Returns the multiplier to convert the carrier integrator value to PPM,
 * for the given radio configuration (namely channel and datarate).
 */
double dw1000_get_hz2ppm_multiplier(const dwt_config_t *dwt_config);

/* Returns the clock frequency offset w.r.t. the sender in ppm for the given 
 * radio configuration, based on the carrier integrator. This function may be 
 * called after a frame reception. For further info, see the documentation for
 * dwt_readcarrierintegrator() of the DecaWave API. 
 *
 */
double dw1000_get_ppm_offset(const dwt_config_t *dwt_config);

/* Returns the XTAL trim value that best compensates the frequency offset.
 *
 * Params:
 *  - curr_offset_ppm  current frequency offset
 *  - curr_trim        current trim values
 */
uint8_t dw1000_get_best_trim_code(double curr_offset_ppm, uint8_t curr_trim);

/* Trim crystal frequency to reduce CFO wrt the last frame received */
bool dw1000_trim();

/* Compute the received signal power levels for the first path and for the
 * overall transmission according to the DW1000 User Manual (4.7.1 "Estimating
 * the signal power in the first path" and 4.7.2 "Estimating the receive signal
 * power").
 * Requires the current configuration to compute PRF and SFD
 * corrections.  Results are stored in the dw1000_diagnostics_t structure.
 *
 * False is returned if power levels could not be computed.
 */
bool dw1000_rxpwr(dw1000_rxpwr_t *d, const dwt_rxdiag_t* rxdiag, const dwt_config_t* config);

/* Estimates the probability that the received signal is affected by NLOS,
 * based on the analysis of DW1000 diagnostics data. The methodology is
 * inspired by DecaWave's "APS006 PART 3 APPLICATION NOTE -
 * DW1000 Metrics for Estimation of Non Line Of SightOperating Conditions".
 * Params:
 *  - d [out]         results are stored in the dw1000_nlos_t structure.
 *  - rxdiag [in]     diagnostics for the acquired signal.
 *  - samples [in]    the CIR window to search for undetected paths.
 *                    The window must precede the detected first path.
 *  - n_samples [in]  length of the CIR window.
 *                    The recommended number of samples is 16.
 */
void dw1000_nlos(dw1000_nlos_t *d, const dwt_rxdiag_t* rxdiag, const dw1000_cir_sample_t* samples, uint16_t n_samples);

/*---------------------------------------------------------------------------*/
#endif //DW1000_UTIL_H_
