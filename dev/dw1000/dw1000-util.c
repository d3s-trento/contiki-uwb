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
