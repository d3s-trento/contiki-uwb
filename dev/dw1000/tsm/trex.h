#ifndef TREX_H
#define TREX_H

enum trex_status {
  TREX_NONE,            // there was no operation, so no status  
  TREX_RX_SUCCESS,      // all good, packet data is loaded to the buffer
  TREX_RX_TIMEOUT,      // radio reported RX timeout
  TREX_RX_ERROR,        // radio reported RX error
  TREX_RX_MALFORMED,    // packet was received but its format is incorrect
  TREX_TIMER_EVENT,     // Trex timer triggered
  TREX_TX_DONE          // transmission has been performed
};

#define TREX_IS_RX_STATUS(status) ((status)>=TREX_RX_SUCCESS && (status)<=TREX_RX_MALFORMED)

#endif //TREX_H
