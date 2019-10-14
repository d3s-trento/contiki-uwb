
#include "dw1000-config.h"
#include "deca_device_api.h"
#include "dw1000-arch.h"
#include <stdio.h>

/* Buffered configuration */
static dwt_config_t   current_cfg;
static dwt_txconfig_t current_tx_cfg;
static uint16_t current_tx_ant_dly;
static uint16_t current_rx_ant_dly;
static bool current_smart_power;

/* Recommended PG delay values */
const uint8_t pgdly_tbl[] = {
  0xC9, // ch. 1
  0xC2, // ch. 2
  0xC5, // ch. 3
  0x95, // ch. 4
  0xC0, // ch. 5
  0x93, // ch. 7
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
dw1000_configure(dwt_config_t *cfg)
{
  int8_t irq_status = dw1000_disable_interrupt();

  dwt_forcetrxoff();
  current_cfg = *cfg;
  dwt_configure(&current_cfg);
  
  dw1000_enable_interrupt(irq_status);

  return true;
}

/* Configure only the TX parameters of the radio */
void 
dw1000_configure_tx(const dwt_txconfig_t* tx_cfg, bool smart) {
  int8_t irq_status = dw1000_disable_interrupt();

  current_tx_cfg = *tx_cfg;
  dwt_configuretxrf(&current_tx_cfg);
  current_smart_power = smart;
  dwt_setsmarttxpower(current_smart_power);
  
  dw1000_enable_interrupt(irq_status);
}

/* Configure antenna delays */
void 
dw1000_configure_ant_dly(uint16_t rx_dly, uint16_t tx_dly) {
  int8_t irq_status = dw1000_disable_interrupt();

  current_rx_ant_dly = rx_dly;
  current_tx_ant_dly = tx_dly;
  dwt_setrxantennadelay(current_rx_ant_dly);
  dwt_settxantennadelay(current_tx_ant_dly);
  
  dw1000_enable_interrupt(irq_status);
}


/* Configures the radio with the pre-defined default parameters */
void 
dw1000_reset_cfg() {

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
  tmp.power = DW1000_CONF_PG_DELAY;
#endif
  dw1000_configure_tx(&tmp, smart);

  // set antenna delays
  dw1000_configure_ant_dly(DW1000_CONF_RX_ANT_DLY, DW1000_CONF_TX_ANT_DLY);
}


/* Get the recommended tx config for the given radio config and the smart TX 
 * power control feature */
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
  return &current_cfg;
}

/* Get the current (cached) TX configuration */
const dwt_txconfig_t*
dw1000_get_current_tx_cfg() {
  return &current_tx_cfg;
}

/* Get the current (cached) antenna delays */
void
dw1000_get_current_ant_dly(uint16_t* rx_dly, uint16_t* tx_dly) {
  *rx_dly = current_rx_ant_dly;
  *tx_dly = current_tx_ant_dly;
}

/* Get the current (cached) status of the Smart TX power control feature */
bool dw1000_is_smart_tx_enabled() {
  return current_smart_power;
}

/* Print the current configuration */
void
dw1000_print_cfg() {
  printf("DW1000 Radio Configuration: \n");
  printf("  Channel: %u\n",        current_cfg.chan);
  printf("  PRF: %u\n",            current_cfg.prf);
  printf("  PLEN: %u\n",           current_cfg.txPreambLength);
  printf("  PAC Size: %u\n",       current_cfg.rxPAC);
  printf("  TX Pre Code: %u\n",    current_cfg.txCode);
  printf("  RX Pre Code: %u\n",    current_cfg.rxCode);
  printf("  Non-std SFD: %u\n",    current_cfg.nsSFD);
  printf("  Data Rate: %u\n",      current_cfg.dataRate);
  printf("  PHR Mode: %u\n",       current_cfg.phrMode);
  printf("  SFD Timeout: %u\n",    current_cfg.sfdTO);
  printf("  Smart TX power: %u\n", current_smart_power);
  printf("  PG Delay: %x\n",       current_tx_cfg.PGdly);
  printf("  TX Power: %lx\n",      current_tx_cfg.power);
  printf("  RX ant delay: %u\n",   current_rx_ant_dly);
  printf("  TX ant delay: %u\n",   current_tx_ant_dly);
}


