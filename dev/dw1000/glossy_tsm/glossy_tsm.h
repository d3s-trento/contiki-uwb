#ifndef GLOSSY_TSM_H
#define GLOSSY_TSM_H

#include <stdbool.h>
#include <stdint.h>

#include "contiki.h"

#include PROJECT_CONF_H

#ifndef GLOSSY_MAX_JITTER_MULT
#define GLOSSY_MAX_JITTER_MULT (125)
#endif

#ifndef GLOSSY_JITTER_STEP
#define GLOSSY_JITTER_STEP     (0x2)
#endif

struct glossy_context_t {
  uint8_t received_len;
  enum {
    GLOSSY_RX_SUCCESS,
    GLOSSY_RX_ERROR,
    GLOSSY_RX_TIMEOUT,
    GLOSSY_RX_UNINITIALIZED // TODO: Use just as check, could remove later
  } rx_status;

  uint8_t n_tx;

  uint32_t logic_deadline;
  uint32_t deadline;

  uint32_t original_preamble;

#if GLOSSY_LATENCY_LOG
  uint32_t start_4ns;
  bool first_rx;
#endif
};

struct glossy_next_action_t {
  bool is_rx;
  bool update_tref;
  uint32_t max_len;
  uint8_t N;
  uint8_t * buffer;
  uint8_t data_len;
  bool jitter;
};

// inline __attribute__((always_inline)) char glossy_tx(uint8_t max_len, uint8_t N, uint8_t * const buffer, uint8_t data_len) {
//   return glossy_trx(false, false, max_len, N, buffer, data_len);
// }
// 
// inline __attribute__((always_inline)) char glossy_rx(uint8_t max_len, uint8_t N, uint8_t * buffer, bool update_tref) {
//   return glossy_trx(true, update_tref, max_len, N, buffer, 0);
// }

extern struct glossy_next_action_t glossy_next_action;
extern struct glossy_context_t glossy_context;
extern struct pt glossy_pt;

#define GLOSSY_TX(_pt, _child, _max_len, _N, _buffer, _data_len, _jitter) do {\
                                                                     glossy_next_action.is_rx = false;\
                                                                     glossy_next_action.update_tref=false;\
                                                                     glossy_next_action.max_len=_max_len;\
                                                                     glossy_next_action.buffer=_buffer;\
                                                                     glossy_next_action.data_len=_data_len;\
                                                                     glossy_next_action.N=_N;\
                                                                     glossy_next_action.jitter=_jitter;\
                                                                     PT_SPAWN(_pt,_child,glossy_trx());} while(0);

#define GLOSSY_RX(_pt, _child, _max_len, _N, _buffer, _update_tref, _jitter) do {\
                                                                     glossy_next_action.is_rx = true;\
                                                                     glossy_next_action.update_tref=_update_tref;\
                                                                     glossy_next_action.max_len=_max_len;\
                                                                     glossy_next_action.buffer=_buffer;\
                                                                     glossy_next_action.data_len=0;\
                                                                     glossy_next_action.N=_N;\
                                                                     glossy_next_action.jitter=_jitter;\
                                                                     PT_SPAWN(_pt,_child,glossy_trx());} while(0);

//char glossy_tx(uint8_t max_len, uint8_t N, uint8_t * const buffer, uint8_t data_len);
//char glossy_rx(uint8_t max_len, uint8_t N, uint8_t * buffer, bool update_tref);
char glossy_trx(/*bool _is_rx, bool _update_tref, uint8_t _max_len, uint8_t _N, uint8_t * _buffer, uint8_t _data_len*/);

#ifndef SINK_RADIUS
#error "The maximum expected distance from the sink (SINK_RADIUS) must be set (usually in a project-conf.h)"
#endif

#endif
