/*
 * Copyright (c) 2019, University of Trento.
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
 *      Contiki DW1000 configuration module
 *
 * \author
 *      Timofei Istomin <tim.ist@gmail.com>
 */

#include <stdio.h>
#include "dw1000-config.h"
#include "deca_device_api.h"
#include "deca_param_types.h"
#include "dw1000-arch.h"
#include "dw1000-shared-state.h"
#include "dw1000-config-struct.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...) do {} while(0)
#endif

/* Cached configuration of the radio */
struct dw1000_all_config dw1000_cached_config;
// just a local alias
#define CUR_CFG dw1000_cached_config


/* Recommended PG delay values */
const uint8_t pgdly_tbl[] = {
  TC_PGDELAY_CH1, // ch. 1
  TC_PGDELAY_CH2, // ch. 2
  TC_PGDELAY_CH3, // ch. 3
  TC_PGDELAY_CH4, // ch. 4
  TC_PGDELAY_CH5, // ch. 5
  TC_PGDELAY_CH7, // ch. 7
};

/* Recommended TX power values */
const uint32_t tx_power_tbl[2][6][2] = {
  { // manual power: prf16, prf64
    {0x75757575, 0x67676767}, // ch. 1
    {0x75757575, 0x67676767}, // ch. 2
    {0x6F6F6F6F, 0x8B8B8B8B}, // ch. 3
    {0x5F5F5F5F, 0x9A9A9A9A}, // ch. 4
    {0x48484848, 0x85858585}, // ch. 5
    {0x92929292, 0xD1D1D1D1}  // ch. 7
  },
  { // smart power: prf16, prf64
    {0x15355575, 0x07274767}, // ch. 1
    {0x15355575, 0x07274767}, // ch. 2
    {0x0F2F4F6F, 0x2B4B6B8B}, // ch. 3
    {0x1F1F3F5F, 0x3A5A7A9A}, // ch. 4
    {0x0E082848, 0x25456585}, // ch. 5
    {0x32527292, 0x5171B1D1}  // ch. 7
  }
};

/* Configure the radio.
 *  - cfg: radio config structure of the DecaWave API
 *  
 * Note that it turns the radio OFF and resets any requested/ongoing 
 * operation.
 *
 * If returns false, the radio configuration is undefined.
 */
bool
dw1000_configure(const dwt_config_t *cfg) {
  
  if (dw1000_is_sleeping) {
    PRINTF("Err: Radio configure requested while sleeping\n");
    return 0;
  }

  int8_t irq_status = dw1000_disable_interrupt();

  dwt_forcetrxoff();
  CUR_CFG.cfg = *cfg;
  dwt_configure(&CUR_CFG.cfg);
  
  dw1000_enable_interrupt(irq_status);

  return true;
}

/* Configure only the channel and TX/RX codes of the radio
 *
 * Note that it turns the radio OFF and resets any requested/ongoing
 * operation.
 *
 * If returns false, the radio configuration is undefined.
 */
bool
dw1000_configure_ch(uint8_t chan, uint8_t txCode, uint8_t rxCode) {
  
  if (dw1000_is_sleeping) {
    PRINTF("Err: Channel configure requested while sleeping\n");
    return 0;
  }
  
  int8_t irq_status = dw1000_disable_interrupt();

  // assume standard SFD
  uint8 nsSfd_result = 0;
  uint8 useDWnsSFD = 0;

  //dwt_forcetrxoff();

  // Configure PLL2/RF PLL block CFG/TUNE (for a given channel);
  // these steps are avoided if switching between ch. 2 and 4, or 5 and 7
  if(fs_pll_cfg[chan_idx[CUR_CFG.cfg.chan]] != fs_pll_cfg[chan_idx[chan]])
    dwt_write32bitoffsetreg(FS_CTRL_ID, FS_PLLCFG_OFFSET, fs_pll_cfg[chan_idx[chan]]);
  if(fs_pll_tune[chan_idx[CUR_CFG.cfg.chan]] != fs_pll_tune[chan_idx[chan]])
    dwt_write8bitoffsetreg(FS_CTRL_ID, FS_PLLTUNE_OFFSET, fs_pll_tune[chan_idx[chan]]);

  // Configure RF RX blocks (for specified channel/bandwidth - wide or narrow);
  uint8 current_bw = ((CUR_CFG.cfg.chan == 4) || (CUR_CFG.cfg.chan == 7)) ? 1 : 0;
  uint8 bw = ((chan == 4) || (chan == 7)) ? 1 : 0;
  if(current_bw != bw) {
    dwt_write8bitoffsetreg(RF_CONF_ID, RF_RXCTRLH_OFFSET, rx_config[bw]);
  }

  // Configure RF TX blocks (for specified channel and PRF)
  // Configure RF TX control
  if(CUR_CFG.cfg.chan != chan)
    dwt_write32bitoffsetreg(RF_CONF_ID, RF_TXCTRL_OFFSET, tx_config[chan_idx[chan]]);

  // Setup of channel control register
  uint32 regval;
  regval =  (CHAN_CTRL_TX_CHAN_MASK & (chan << CHAN_CTRL_TX_CHAN_SHIFT)) | // Transmit Channel
            (CHAN_CTRL_RX_CHAN_MASK & (chan << CHAN_CTRL_RX_CHAN_SHIFT)) | // Receive Channel
            (CHAN_CTRL_RXFPRF_MASK & ((uint32)CUR_CFG.cfg.prf << CHAN_CTRL_RXFPRF_SHIFT)) | // RX PRF
            ((CHAN_CTRL_TNSSFD|CHAN_CTRL_RNSSFD) & ((uint32)nsSfd_result << CHAN_CTRL_TNSSFD_SHIFT)) | // nsSFD enable RX&TX
            (CHAN_CTRL_DWSFD & ((uint32)useDWnsSFD << CHAN_CTRL_DWSFD_SHIFT)) | // Use DW nsSFD
            (CHAN_CTRL_TX_PCOD_MASK & ((uint32)txCode << CHAN_CTRL_TX_PCOD_SHIFT)) | // TX Preamble Code
            (CHAN_CTRL_RX_PCOD_MASK & ((uint32)rxCode << CHAN_CTRL_RX_PCOD_SHIFT)) ; // RX Preamble Code
  dwt_write32bitreg(CHAN_CTRL_ID, regval) ;

  // initiate and abort a transmission to initialise the SFD
  dwt_write8bitoffsetreg(SYS_CTRL_ID, SYS_CTRL_OFFSET, SYS_CTRL_TXSTRT | SYS_CTRL_TRXOFF);

  // update current saved config
  CUR_CFG.cfg.chan = chan;
  CUR_CFG.cfg.txCode = txCode;
  CUR_CFG.cfg.rxCode = rxCode;

  dw1000_enable_interrupt(irq_status);

  return true;
}

/* Configure only the TX parameters of the radio */
bool 
dw1000_configure_tx(const dwt_txconfig_t* tx_cfg, bool smart) {
  if (dw1000_is_sleeping) {
    PRINTF("Err: TX configure requested while sleeping\n");
    return 0;
  }
  int8_t irq_status = dw1000_disable_interrupt();

  CUR_CFG.tx_cfg = *tx_cfg;
  dwt_configuretxrf(&CUR_CFG.tx_cfg);
  CUR_CFG.smart_power = smart;
  dwt_setsmarttxpower(CUR_CFG.smart_power);
  
  dw1000_enable_interrupt(irq_status);
  return 1;
}

/* Configure antenna delays */
bool
dw1000_configure_ant_dly(uint16_t rx_dly, uint16_t tx_dly) {
  if (dw1000_is_sleeping) {
    PRINTF("Err: Ant dly configure requested while sleeping\n");
    return 0;
  }
  int8_t irq_status = dw1000_disable_interrupt();

  CUR_CFG.rx_ant_dly = rx_dly;
  CUR_CFG.tx_ant_dly = tx_dly;
  dwt_setrxantennadelay(CUR_CFG.rx_ant_dly);
  dwt_settxantennadelay(CUR_CFG.tx_ant_dly);
  
  dw1000_enable_interrupt(irq_status);
  return 1;
}


/* Configures the radio with the pre-defined default parameters */
bool
dw1000_reset_cfg() {
  if (dw1000_is_sleeping) {
    PRINTF("Err: Reset cfg requested while sleeping\n");
    return 0;
  }
  
  // use the configuration constants
  dwt_config_t default_cfg = {
    .chan = DW1000_CHANNEL,        
    .prf = DW1000_PRF,
    .txPreambLength = DW1000_PLEN,
    .rxPAC = DW1000_PAC,
    .txCode = DW1000_PREAMBLE_CODE,
    .rxCode = DW1000_PREAMBLE_CODE,
    .nsSFD = DW1000_SFD_MODE,
    .dataRate = DW1000_DATA_RATE,
    .phrMode = DW1000_PHR_MODE,
    .sfdTO = DW1000_SFD_TIMEOUT,
  };
  dw1000_configure(&default_cfg);

  bool smart;

  if (DW1000_DATA_RATE==DWT_BR_6M8) {
    // only makes sense for 6M8
    smart = DW1000_SMART_TX_POWER_6M8;
  }
  else {
    smart = false;
  }

  dwt_txconfig_t tmp;

  // get the recommended values
  dw1000_get_recommended_tx_cfg(&default_cfg, smart, &tmp);

  // force the tx power and pg delay settings from the configuration constants 
  // if they are defined, otherwise take the recommended value

#if defined(DW1000_CONF_TX_POWER)
  tmp.power = DW1000_CONF_TX_POWER;
#endif
#if defined(DW1000_CONF_PG_DELAY)
  tmp.PGdly = DW1000_CONF_PG_DELAY;
#endif
  dw1000_configure_tx(&tmp, smart);

  // set antenna delays
  dw1000_configure_ant_dly(DW1000_CONF_RX_ANT_DLY, DW1000_CONF_TX_ANT_DLY);

  return 1;
}


/* Get the recommended tx config for the given radio config and the smart TX 
 * power control feature. 
 *
 * Returns false if the requested configuration is not supported.
 */
bool
dw1000_get_recommended_tx_cfg(const dwt_config_t* const cfg, bool smart, dwt_txconfig_t* tx_cfg) {
  int ch_idx;
  int prf_idx;
  int smart_idx;

  if (smart && (cfg->dataRate!=DWT_BR_6M8)) {
    return false;
  }

  if (cfg->chan < 1 || cfg->chan > 7 || cfg->chan == 6) {
    return false;
  }

  if (cfg->prf == DWT_PRF_16M) {
    prf_idx = 0;
  }
  else if (cfg->prf == DWT_PRF_64M) {
    prf_idx = 1;
  }
  else {
    return false;
  }

  if (cfg->chan == 7) {
    ch_idx = 5;
  }
  else {
    ch_idx = cfg->chan-1;
  }

  smart_idx = smart?1:0;

  tx_cfg->PGdly = pgdly_tbl[ch_idx];
  tx_cfg->power = tx_power_tbl[smart_idx][ch_idx][prf_idx];

  return true;
}

/* Applies the recommended TX parameters for the current radio configuration
 * and the requested smart TX power control feature.
 *
 * Returns false if the requested configuration is not supported.
 */
bool
dw1000_set_recommended_tx_cfg(bool smart) {
  dwt_txconfig_t tmp;

  if (!dw1000_get_recommended_tx_cfg(&CUR_CFG.cfg, smart, &tmp)) {
    return false;
  }
  
  return dw1000_configure_tx(&tmp, smart);
}


/* Perform the same checks as above about the static configuration */

#if((DW1000_CHANNEL) < 1 || (DW1000_CHANNEL) > 7 || (DW1000_CHANNEL) == 6 )
#error Invalid value of DW1000_CHANNEL
#endif

#if((DW1000_PRF) != DWT_PRF_16M && (DW1000_PRF) != DWT_PRF_64M )
#error Invalid value of DW1000_PRF
#endif


/* Get the current (cached) radio configuration */
const dwt_config_t*
dw1000_get_current_cfg() {
  return &CUR_CFG.cfg;
}

/* Get the current (cached) TX configuration */
const dwt_txconfig_t*
dw1000_get_current_tx_cfg() {
  return &CUR_CFG.tx_cfg;
}

/* Get the current (cached) antenna delays */
void
dw1000_get_current_ant_dly(uint16_t* rx_dly, uint16_t* tx_dly) {
  *rx_dly = CUR_CFG.rx_ant_dly;
  *tx_dly = CUR_CFG.tx_ant_dly;
}

/* Restore antenna delay configuration after wake-up */
bool
dw1000_restore_ant_delay(void)
{
  return dw1000_configure_ant_dly(CUR_CFG.rx_ant_dly, CUR_CFG.tx_ant_dly);
}

/* Get the current (cached) status of the Smart TX power control feature */
bool dw1000_is_smart_tx_enabled() {
  return CUR_CFG.smart_power;
}

/* Print the current configuration */
void
dw1000_print_cfg() {
  printf("DW1000 Radio Configuration: \n");
  printf("  Channel: %u\n",        CUR_CFG.cfg.chan);
  printf("  PRF: %u\n",            CUR_CFG.cfg.prf);
  printf("  PLEN: %u\n",           CUR_CFG.cfg.txPreambLength);
  printf("  PAC Size: %u\n",       CUR_CFG.cfg.rxPAC);
  printf("  TX Pre Code: %u\n",    CUR_CFG.cfg.txCode);
  printf("  RX Pre Code: %u\n",    CUR_CFG.cfg.rxCode);
  printf("  Non-std SFD: %u\n",    CUR_CFG.cfg.nsSFD);
  printf("  Data Rate: %u\n",      CUR_CFG.cfg.dataRate);
  printf("  PHR Mode: %u\n",       CUR_CFG.cfg.phrMode);
  printf("  SFD Timeout: %u\n",    CUR_CFG.cfg.sfdTO);
  printf("  Smart TX power: %u\n", CUR_CFG.smart_power);
  printf("  PG Delay: %x\n",       CUR_CFG.tx_cfg.PGdly);
  printf("  TX Power: %lx\n",      CUR_CFG.tx_cfg.power);
  printf("  RX ant delay: %u\n",   CUR_CFG.rx_ant_dly);
  printf("  TX ant delay: %u\n",   CUR_CFG.tx_ant_dly);
}

#if UWB_CONTIKI_PRINT_DEF
#include "print-def.h"

#pragma message STRDEF(DW1000_CHANNEL)
#pragma message STRDEF(DW1000_PRF)
#pragma message STRDEF(DW1000_PLEN)
#pragma message STRDEF(DW1000_PAC)
#pragma message STRDEF(DW1000_PREAMBLE_CODE)
#pragma message STRDEF(DW1000_SFD_MODE)
#pragma message STRDEF(DW1000_PHR_MODE)
#pragma message STRDEF(DW1000_DATA_RATE)
#pragma message STRDEF(DW1000_SFD_TIMEOUT)
#pragma message STRDEF(DW1000_SMART_TX_POWER_6M8)
#pragma message STRDEF(DW1000_CONF_TX_POWER)
#pragma message STRDEF(DW1000_CONF_PG_DELAY)
#endif
