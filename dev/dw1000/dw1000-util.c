/*
 * Copyright (c) 2017, University of Trento.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/**
 * \file
 *      Various DW1000 Utils
 *
 * \author
 *      Davide Vecchia <davide.vecchia@unitn.it>
 *      Timofei Istomin <tim.ist@gmail.com>
 */
#include "deca_device_api.h"
#include "dw1000-arch.h"
#include "dw1000-cir.h"
#include "dw1000-util.h"
#include "dw1000-config.h"
#include <math.h>
/*---------------------------------------------------------------------------*/
#define POW2(x) ((x)*(x))

// Compute the square of the magnitude of the complex CIR sample
#define MAGSQ(x) (POW2(x.compl.real) + POW2(x.compl.imag))
/*---------------------------------------------------------------------------*/
static double cfo_jitter_guard = DW1000_CFO_JITTER_GUARD;
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
uint32_t
dw1000_estimate_tx_time(const dwt_config_t* dwt_config, uint16_t framelength, bool only_rmarker)
{

  uint32_t tx_time      = 0;
  size_t sym_timing_ind = 0;
  uint16_t shr_len      = 0;

  const uint16_t DATA_BLOCK_SIZE  = 330;
  const uint16_t REED_SOLOM_BITS  = 48;

  // Symbol timing LUT
  const size_t SYM_TIM_16MHZ = 0;
  const size_t SYM_TIM_64MHZ = 9;
  const size_t SYM_TIM_110K  = 0;
  const size_t SYM_TIM_850K  = 3;
  const size_t SYM_TIM_6M8   = 6;
  const size_t SYM_TIM_SHR   = 0;
  const size_t SYM_TIM_PHR   = 1;
  const size_t SYM_TIM_DAT   = 2;

  const static uint16_t SYM_TIM_LUT[] = {
    // 16 Mhz PRF
    994, 8206, 8206,  // 0.11 Mbps
    994, 1026, 1026,  // 0.85 Mbps
    994, 1026, 129,   // 6.81 Mbps
    // 64 Mhz PRF
    1018, 8206, 8206, // 0.11 Mbps
    1018, 1026, 1026, // 0.85 Mbps
    1018, 1026, 129   // 6.81 Mbps
  };

  // Find the PHR
  switch( dwt_config->prf ) {
    case DWT_PRF_16M:  sym_timing_ind = SYM_TIM_16MHZ; break;
    case DWT_PRF_64M:  sym_timing_ind = SYM_TIM_64MHZ; break;
  }

  // Find the preamble length
  switch( dwt_config->txPreambLength ) {
    case DWT_PLEN_64:    shr_len = 64;    break;
    case DWT_PLEN_128:   shr_len = 128;   break;
    case DWT_PLEN_256:   shr_len = 256;   break;
    case DWT_PLEN_512:   shr_len = 512;   break;
    case DWT_PLEN_1024:  shr_len = 1024;  break;
    case DWT_PLEN_1536:  shr_len = 1536;  break;
    case DWT_PLEN_2048:  shr_len = 2048;  break;
    case DWT_PLEN_4096:  shr_len = 4096;  break;
  }

  // Find the datarate
  switch( dwt_config->dataRate ) {
    case DWT_BR_110K:
      sym_timing_ind  += SYM_TIM_110K;
      shr_len         += 64;  // SFD 64 symbols
      break;
    case DWT_BR_850K:
      sym_timing_ind  += SYM_TIM_850K;
      shr_len         += 8;   // SFD 8 symbols
      break;
    case DWT_BR_6M8:
      sym_timing_ind  += SYM_TIM_6M8;
      shr_len         += 8;   // SFD 8 symbols
      break;
  }

  // Add the SHR time
  tx_time   = shr_len * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_SHR ];

  // If not only RMARKER, calculate PHR and data
  if( !only_rmarker ) {

    // Add the PHR time (21 bits)
    tx_time  += 21 * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_PHR ];

    // Bytes to bits
    framelength *= 8;

    // Add Reed-Solomon parity bits
    framelength += REED_SOLOM_BITS * ( framelength + DATA_BLOCK_SIZE - 1 ) / DATA_BLOCK_SIZE;

    // Add the DAT time
    tx_time += framelength * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_DAT ];

  }

  // Return in nano seconds
  return tx_time;

}
/*---------------------------------------------------------------------------*/
double dw1000_get_hz2ppm_multiplier(const dwt_config_t *dwt_config) {
    double freq_offs;
    double hz2ppm;
    
    if (dwt_config->dataRate == DWT_BR_110K) {
        freq_offs = FREQ_OFFSET_MULTIPLIER_110KB;
    } else {
        freq_offs = FREQ_OFFSET_MULTIPLIER;
    }
    // use the value corresponding to center frequency for the
    // given channel (section 10.5 dw1000 user manual):
    // 1,2 and 5 have different frequencies
    // 4 has the same of channel 2
    // 7 has the same of channel 5
    switch (dwt_config->chan) {
        case 1:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_1; break;
        case 2:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_2; break;
        case 3:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_3; break;
        case 4:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_2; break;
        case 5:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_5; break;
        case 7:  hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_5; break;
        default: hz2ppm = HERTZ_TO_PPM_MULTIPLIER_CHAN_5;
    }
    return freq_offs*hz2ppm;
}
/*---------------------------------------------------------------------------*/
double
dw1000_get_ppm_offset(const dwt_config_t *dwt_config)
{
    int32_t ci = dwt_readcarrierintegrator();
    return ci * dw1000_get_hz2ppm_multiplier(dwt_config);
}
/*---------------------------------------------------------------------------*/
void
dw1000_set_cfo_jitter_guard(double ppm)
{
    cfo_jitter_guard = ppm;
}
/*---------------------------------------------------------------------------*/
uint8_t
dw1000_get_best_trim_code(double curr_offset_ppm, uint8_t curr_trim)
{
    if (curr_offset_ppm > DW1000_CFO_PPM_PER_TRIM/2+cfo_jitter_guard ||
        curr_offset_ppm < -DW1000_CFO_PPM_PER_TRIM/2-cfo_jitter_guard
        ) {
        // estimate in PPM
        int8_t trim_adjust = (int8_t)round(
          (double)(DW1000_CFO_WANTED + curr_offset_ppm)
          / (double)DW1000_CFO_PPM_PER_TRIM);
        // printf("ppm %d guard %d trim %u adj %d\n",
        //   (int)(curr_offset_ppm * 1000), (int)(cfo_jitter_guard * 1000),
        //   (unsigned int)curr_trim, (int)trim_adjust);
        curr_trim -= trim_adjust;

        if (curr_trim < 1)
            curr_trim = 1;
        else if (curr_trim > 31)
            curr_trim = 31;
    }
    return curr_trim;
}
/*--------------------------------------------------------------------------*/
/* Trim crystal frequency to reduce CFO wrt the last frame received */
bool
dw1000_trim() {
  double ppm_offset = dw1000_get_ppm_offset(dw1000_get_current_cfg());
  uint8_t current_trim_code = dwt_getxtaltrim();
  uint8_t new_trim_code = dw1000_get_best_trim_code(ppm_offset, current_trim_code);
  if(new_trim_code != current_trim_code) {
    dw1000_spi_set_slow_rate();
    dwt_setxtaltrim(new_trim_code);
    dw1000_spi_set_fast_rate();
    return 1;
  }
  return 0;
}
/*---------------------------------------------------------------------------*/
/* Compute the received signal power levels for the first path and for the
 * overall transmission according to the DW1000 User Manual (4.7.1 "Estimating
 * the signal power in the first path" and 4.7.2 "Estimating the receive signal
 * power").
 * Requires the current configuration to compute PRF and SFD
 * corrections.  Results are stored in the dw1000_diagnostics_t structure.
 *
 * False is returned if power levels could not be computed.
 */
bool dw1000_rxpwr(dw1000_rxpwr_t *d, const dwt_rxdiag_t* rxdiag, const dwt_config_t* config) {

  /* Compute corrected preamble counter (used for CIR power adjustment) */
  int16_t corrected_pac;
  if(rxdiag->rxPreamCount == rxdiag->pacNonsat) {
    // NOTE this is only valid for standard SFDs!
    int16_t sfd_correction = (config->dataRate == DWT_BR_110K) ? 64 : 5;
    corrected_pac = rxdiag->rxPreamCount - sfd_correction;
  }
  else {
    corrected_pac = rxdiag->rxPreamCount;
  }

  d->pac_correction = POW2(corrected_pac);

  /* Compute the CIR power level, corrected by PAC value */
  d->cir_pwr_norm = (double)rxdiag->maxGrowthCIR * ((uint32_t)1 << 17) / d->pac_correction;

  /* Compute RX power and First-Path RX power */
  d->fp_raw = (double)(POW2(rxdiag->firstPathAmp1) + POW2(rxdiag->firstPathAmp2) + POW2(rxdiag->firstPathAmp3)) / d->pac_correction;
  if(d->cir_pwr_norm != 0 && d->fp_raw != 0) {
    double prf_correction = (config->prf == DWT_PRF_64M) ? 121.74 : 113.77;
    d->rx_pwr = 10 * log10(d->cir_pwr_norm) - prf_correction;
    d->fp_pwr = 10 * log10(d->fp_raw) - prf_correction;
  }
  else {
    d->rx_pwr = 0;
    d->fp_pwr = 0;
    return false;
  }

  return true;
}
/*---------------------------------------------------------------------------*/
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
void dw1000_nlos(dw1000_nlos_t *d, const dwt_rxdiag_t* rxdiag, const dw1000_cir_sample_t* samples, uint16_t n_samples) {

  /* --- Step 1: extract NLOS probability based on the distance between the first path and peak path indexes */

  /* First, compute the index difference between first path and peak path;
   * firstPath is expressed as a 16-bits fixed point value (10bits.6bits).
   * We obtain the floating point representation simply dividing by 64.
   * The reported peakPath, instead, has no fractional part. */
  double fp = (double)(rxdiag->firstPath) / 64;
  d->path_diff = fabs((double)rxdiag->peakPath - fp);

  /* Derive NLOS probability based on the difference between first and peak path;
   * the computation for pr_nlos is provided by the manufacturer. */
  if(d->path_diff <= 3.3)
    d->pr_nlos = 0.0;
  else if((d->path_diff < 6.0) && (d->path_diff > 3.3))
    d->pr_nlos = 0.39178 * d->path_diff - 1.31719;
  else
    d->pr_nlos = 1.0;

  /* --- Step 2: likelihood of undetected early path */

  /* Lower the threshold for path detection */
  uint8_t ntm, pmult;
  dw1000_get_current_lde_cfg(&ntm, &pmult);
  d->low_noise = rxdiag->stdNoise * ntm * 0.6;

  /* Count the number of candidate undetected paths in the CIR window;
   * if the CIR was not provided, skip this search */
  d->num_early_peaks = 0;
  if(samples != NULL && n_samples > 2) {
    double ampl_prev, ampl, ampl_next;

    /* Start by computing the magnitude of the first CIR sample */
    ampl      = sqrt(MAGSQ(samples[0]));
    ampl_next = sqrt(MAGSQ(samples[1]));

    /* Check all samples in the window, three-by-three, to detect peaks */
    for(int n=2; n<n_samples; n++) {

      /* Extract the magnitudes of the CIR for the previous CIR
       * sample, the current one and the next one */
      ampl_prev = ampl;
      ampl = ampl_next;
      ampl_next = sqrt(MAGSQ(samples[n]));

      /* Identify a candidate peak when the magnitude first rises
       * above the new noise level, and then decreases */
      if(ampl > d->low_noise && ampl-ampl_prev > 0 && ampl_next-ampl < 0) {
        d->num_early_peaks++;
      }
    }

    /* The likelihood depends on the number of candidate early paths vs the window size */
    d->luep = (double)(d->num_early_peaks * 2) / (n_samples - 1);
  }
  else {
    d->luep = 0;
  }

  /* --- Step 3: detect accumulator saturation */

  /* Find the maximum amplitude of the first path (the radio reports 3 values close 
   * to the leading edge) */
  uint16_t firstPathAmp = rxdiag->firstPathAmp1;
  if(rxdiag->firstPathAmp2 > firstPathAmp) firstPathAmp = rxdiag->firstPathAmp2;
  if(rxdiag->firstPathAmp3 > firstPathAmp) firstPathAmp = rxdiag->firstPathAmp3;

  /* When first and peak path have similar magnitudes, the metric is close to 1.0,
   * indicating that saturation has likely occured */
  d->mc = (double)firstPathAmp / (double)rxdiag->peakPathAmp;

  /* --- Step 4: combine metrics in a single indicator (Confidence Level CL = 1 -> LOS). */
  /* 
   * If early paths were detected, the channel is likely NLOS (CL = 0.0);
   * if first and peak path are very close, or saturation happened,
   * channel is likely LOS (CL = 1.0);
   * in other cases, CL depends on the distance between first and peak path. */
  if(d->luep > 0.01) {
    d->cl = 0.0;
  }
  else if(d->pr_nlos < 0.001 || d->mc >= 0.9) {
    d->cl = 1.0;
  }
  else {
    d->cl = 1.0 - d->pr_nlos;
  }
}
/*---------------------------------------------------------------------------*/
