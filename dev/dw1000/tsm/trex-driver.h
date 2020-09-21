#ifndef TREX_DRIVER_H
#define TREX_DRIVER_H

#include <stdint.h>
#include "trex.h"

typedef struct trexs_stats_t {
    // rx_err events
    uint16_t n_phe;      /* PHR errors */
    uint16_t n_sfdto;    /* SFD timeouts */
    uint16_t n_rse;      /* Errors in Reed Solomon decoding phase */
    uint16_t n_fcse;     /* FCS (CRC) errors */
    uint16_t n_rej;      /* Rejections due to frame filtering */
    // rx_timeout events
    uint16_t n_fto;      /* Received Frame Wait Timeout counter */
    uint16_t n_pto;      /* Preamble Detection Timeout counter */
    // other
    uint16_t n_unknown;
    // success
    uint16_t n_rxok;
    uint16_t n_txok;
} trexd_stats_t;

typedef struct {
  uint32_t          trx_sfd_time_4ns;   // SFD time of the last TX or RX
  enum trex_status  status;             // slot operation status
  uint8_t*          buffer;             // packet buffer
  uint8_t           payload_len;        // length of the received or transmitted packet
} trexd_slot_t;

#define TREXD_PLD_OFFS 0

typedef void (*trexd_slot_cb)(const trexd_slot_t* slot);

int trexd_tx_at(uint8_t *buffer, uint8_t payload_len, uint32_t sfd_time_4ns);
int trexd_rx_slot(uint8_t *buffer, uint32_t expected_sfd_time_4ns, uint32_t deadline_4ns);
int trexd_rx_until(uint8_t *buffer, uint32_t deadline_4ns);
int trexd_rx(uint8_t *buffer);
int trexd_rx_from(uint8_t *buffer, uint32_t rx_on_4ns);
int trexd_set_timer(uint32_t deadline_4ns);  // secret undocumented function, use at your own risk ;)
void trexd_set_rx_slot_preambleto_pacs(const uint16_t preambleto_pacs);

void trexd_init();
void trexd_set_slot_callback(trexd_slot_cb callback);
void trexd_stats_get(trexd_stats_t*);
void trexd_stats_print();
void trexd_stats_reset();



#endif //TREX_DRIVER_H
