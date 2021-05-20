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
#include "dw1000-util.h"
#include <math.h>
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

  #define POW2(x) ((x)*(x))
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
