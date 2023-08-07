#ifndef DW1000_CONFIG_H
#define DW1000_CONFIG_H

#include "deca_device_api.h"
#include "deca_regs.h"
#include "deca_param_types.h"
#include <stdint.h>
#include <stdbool.h>
#include "contiki-conf.h"

/*-- Radio configuration ------------------------------------------------------- */

/*
 * Pre-configured default values
 *
 * Channel: 4
 * PRF: 64M
 * Preamble Length (PLEN): 128
 * Preamble Acquisition Count (PAC): 8
 * SFD Mode: Standard-compliant
 * Bit Rate: 6.8 Mbps
 * Physical Header Mode: Standard-compliant
 * SFD Timeout: 129 + 8 - 8
 * TX/RX Preamble Code: 17
 */

#ifdef DW1000_CONF_CHANNEL
#define DW1000_CHANNEL DW1000_CONF_CHANNEL
#else
#define DW1000_CHANNEL 4
#endif

#ifdef DW1000_CONF_PRF
#define DW1000_PRF DW1000_CONF_PRF
#else
#define DW1000_PRF DWT_PRF_64M
#endif

#ifdef DW1000_CONF_PLEN
#define DW1000_PLEN DW1000_CONF_PLEN
#else
#define DW1000_PLEN DWT_PLEN_128
#endif

#ifdef DW1000_CONF_PAC
#define DW1000_PAC DW1000_CONF_PAC
#else
#define DW1000_PAC DWT_PAC8
#endif

#ifdef DW1000_CONF_SFD_MODE
#define DW1000_SFD_MODE DW1000_CONF_SFD_MODE
#else
#define DW1000_SFD_MODE 0 // 0=standard
#endif

#ifdef DW1000_CONF_DATA_RATE
#define DW1000_DATA_RATE DW1000_CONF_DATA_RATE
#else
#define DW1000_DATA_RATE DWT_BR_6M8
#endif

#ifdef DW1000_CONF_PHR_MODE
#define DW1000_PHR_MODE DW1000_CONF_PHR_MODE
#else
#define DW1000_PHR_MODE DWT_PHRMODE_STD
#endif

/* Note that the following values should be computed/selected depending on
 * other parameters configuration. For instance, the preamble code depends on
 * the channel being used. Moreover, for every channel there are at least 2
 * preamble codes available.
 */
#ifdef DW1000_CONF_PREAMBLE_CODE
#define DW1000_PREAMBLE_CODE DW1000_CONF_PREAMBLE_CODE
#else
#define DW1000_PREAMBLE_CODE 17
#endif

#ifdef DW1000_CONF_SFD_TIMEOUT
#define DW1000_SFD_TIMEOUT DW1000_CONF_SFD_TIMEOUT
#else
#define DW1000_SFD_TIMEOUT (128 + 1 + 8 - 8)
#endif


/*-- TX configuration ------------------------------------------------------- */

// Smart TX power control is only available with 6.8 Mbps data rate
 
#ifdef DW1000_CONF_SMART_TX_POWER_6M8
#define DW1000_SMART_TX_POWER_6M8 DW1000_CONF_SMART_TX_POWER_6M8
#else
#define DW1000_SMART_TX_POWER_6M8 1
#endif

/* The following constants might be used to override the recommended values */
// DW1000_CONF_TX_POWER
// DW1000_CONF_PG_DELAY

/*-- LDE configuration --------------------------------------------------- */

#if DW1000_NLOS_OPTIMIZATION

// warning for potentially misused NLOS optimization
#if DW1000_PRF != DWT_PRF_16M
#warning "PRF 16MHz is recommended for NLOS optimization."
#endif
#if DW1000_PLEN == DWT_PLEN_64 || DW1000_PLEN == DWT_PLEN_128 || \
    DW1000_PLEN == DWT_PLEN_256 || DW1000_PLEN == DWT_PLEN_512
#warning "PLEN >=1024 is recommended for NLOS optimization."
#endif

// LDE configuration optimized for NLOS (DecaWave APS006 Part 2, v.1.5)
#define DW1000_LDE_NTM (7)
#define DW1000_LDE_PMULT (0)
#define DW1000_LDE_PRF_TUNE (0x0003)

// warnings for found definitions that will have no effect
#ifdef DW1000_CONF_LDE_NTM
#warning "DW1000_CONF_LDE_NTM will be ignored (NLOS optimization enabled)."
#endif
#ifdef DW1000_CONF_LDE_PMULT
#warning "DW1000_CONF_LDE_PMULT will be ignored (NLOS optimization enabled)."
#endif
#ifdef DW1000_CONF_LDE_PRF_TUNE
#warning "DW1000_CONF_LDE_PRF_TUNE will be ignored (NLOS optimization enabled)."
#endif

#else

// Sub-Register 0x2E:0806 – LDE_CFG1, NTM bits of NTM_1
#ifdef DW1000_CONF_LDE_NTM
#define DW1000_LDE_NTM DW1000_CONF_LDE_NTM
#else
#define DW1000_LDE_NTM N_STD_FACTOR // default: (13)
#endif

// Sub-Register 0x2E:0806 – LDE_CFG1, PMULT bits of NTM_1
#ifdef DW1000_CONF_LDE_PMULT
#define DW1000_LDE_PMULT DW1000_CONF_LDE_PMULT
#else
#define DW1000_LDE_PMULT (PEAK_MULTPLIER >> 5) // default: (0x60 >> 5)
#endif

// Sub-Register 0x2E:1806 – LDE_CFG2, NTM_2
#ifdef DW1000_CONF_LDE_PRF_TUNE
#define DW1000_LDE_PRF_TUNE DW1000_CONF_LDE_PRF_TUNE
#else
#if DW1000_PRF == DWT_PRF_16M
#define DW1000_LDE_PRF_TUNE LDE_PARAM3_16 // default: (0x1607)
#elif DW1000_PRF == DWT_PRF_64M
#define DW1000_LDE_PRF_TUNE LDE_PARAM3_64 // default: (0x0607)
#endif 
#endif

#endif /* DW1000_NLOS_OPTIMIZATION */

/*-- Driver configuration --------------------------------------------------- */

#ifdef DW1000_CONF_DEBUG_LEDS
#define DW1000_DEBUG_LEDS DW1000_CONF_DEBUG_LEDS
#else
#define DW1000_DEBUG_LEDS 1
#endif

/*--------------------------------------------------------------------------- */

/* Configure the radio.
 *  - cfg: radio config structure of the DecaWave API
 *  
 * Note that this call turns the radio OFF and resets any requested/ongoing 
 * operation.
 *
 * If returns false, the radio configuration is undefined.
 */
bool
dw1000_configure(const dwt_config_t *cfg);

/* Change the channel and TX/RX preamble codes */
bool
dw1000_configure_ch(uint8_t chan, uint8_t txCode, uint8_t rxCode);

/* Set a custom non-standard SFD */
bool
dw1000_configure_nssfd(uint16_t polarities, uint16_t magnitudes);

/* Configure only the TX parameters of the radio */
bool
dw1000_configure_tx(const dwt_txconfig_t* tx_cfg, bool smart);

/* Configure only the antenna delays */
bool
dw1000_configure_ant_dly(uint16_t rx_dly, uint16_t tx_dly);

/* Configure LDE */
bool
dw1000_configure_lde(uint8_t ntm, uint8_t pmult, uint16_t prf_tune);

/* Configures the radio with the pre-defined default parameters */
bool
dw1000_reset_cfg();

/* Get the recommended tx config for the given radio config and the smart TX 
 * power control feature. 
 *
 * Returns false if the requested configuration is not supported.
 */
bool
dw1000_get_recommended_tx_cfg(const dwt_config_t* const cfg, bool smart, dwt_txconfig_t* tx_cfg);

/* Applies the recommended TX parameters for the current radio configuration
 * and the requested smart TX power control feature.
 *
 * Returns false if the requested configuration is not supported.
 */
bool
dw1000_set_recommended_tx_cfg(bool smart);

/* Get the current (cached) radio configuration */
const dwt_config_t*
dw1000_get_current_cfg();

/* Get the current (cached) TX configuration */
const dwt_txconfig_t*
dw1000_get_current_tx_cfg();

/* Get the current (cached) antenna delays */
void
dw1000_get_current_ant_dly(uint16_t* rx_dly, uint16_t* tx_dly);

/* Get the current (cached) LDE configuration */
void
dw1000_get_current_lde_cfg(uint8_t* ntm, uint8_t* pmult, uint16_t* prf_tune);

/* Restore the configuration after wake-up */
bool dw1000_restore_config_wa(void);

/* Get the current (cached) status of the Smart TX power control feature */
bool
dw1000_is_smart_tx_enabled();

/* Print the current configuration */
void
dw1000_print_cfg();

#endif //DW1000_CONFIG_H
