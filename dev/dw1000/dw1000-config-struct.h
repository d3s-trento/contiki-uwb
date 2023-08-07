#ifndef DW1000_CONFIG_STRUCT_H
#define DW1000_CONFIG_STRUCT_H

/* Structure holding all the configuration of DW1000.
 *
 * Note that it is intended only for internal use of dw1000 driver.
 */

struct dw1000_all_config {
  dwt_config_t   cfg;
  dwt_txconfig_t tx_cfg;
  uint16_t tx_ant_dly;
  uint16_t rx_ant_dly;
  bool smart_power;
  uint8_t lde_ntm;
  uint8_t lde_pmult;
  uint16_t lde_prf_tune;
  bool nssfd;
  bool custom_nssfd;
  uint16_t nssfd_polarity;
  uint16_t nssfd_magnitude;
};

#endif //DW1000_CONFIG_STRUCT_H
