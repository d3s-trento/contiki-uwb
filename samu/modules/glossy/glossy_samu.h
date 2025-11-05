/*
 * Copyright (c) 2025, University of Trento, Italy
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * \file SAMU Glossy API
 *
 * Authors:
 *   Enrico Soprana <enrico.soprana@unitn.it>
 */
#ifndef GLOSSY_SAMU_H
#define GLOSSY_SAMU_H

#include <stdbool.h>
#include <stdint.h>

#include "contiki.h"

#include "samu-ral-status.h"
#include "samu-ral.h"
#include "samu.h"

#include PROJECT_CONF_H

#ifndef GLOSSY_MAX_JITTER_MULT
#define GLOSSY_MAX_JITTER_MULT (125)
#endif

#ifndef GLOSSY_JITTER_STEP
#define GLOSSY_JITTER_STEP (0x2)
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
  bool is_rx; // Used to specify it the node should transmit or receive (and
              // propagate)
  bool update_tref; // Used to specify whether the node should synchronize using
                    // the received flood (used only when receiving) default:
                    // false
  uint32_t max_len; // Used to specify the maximum number of actions to use for
                    // the Glossy flood default: None, should always be set
  uint8_t N;       // Number of repetitions for the retransmission of the packet
                   // default: None, should always be set
  uint8_t *buffer; // Buffer to use for the packet (received or transmitted)
                   // default: None, should always be set
  uint8_t data_len; // Length of the data to transmit (used when transmitting)
                    // default: None, should always be set
  bool jitter; // Used to enable or disable the jitter used for retransmission
               // default: false
  uint8_t slivers_per_action;    // Used to specify the number of slots
                                 // corresponding to a base action (RX or TX)
                                 // default: SAMU_DEFAULT_SLIVERS_PER_ACTION
  uint32_t rx_guard_time_before; // wake up a bit earlier to compensate for
                                 // potential clock drift only meaningful for RX
                                 // slots default: SAMU_DEFAULT_RXGUARD
  uint32_t rx_guard_time_after; // Remain listening for a possible preamble for
                                // more time to compensate for potential clock
                                // drift default: 0
  uint32_t rx_timeout; // Maximum listening time not including SHR (preamble and
                       // SFD) default: samu_get_default_rx_timeout()
};

extern struct glossy_next_action_t glossy_next_action;
extern struct glossy_context_t glossy_context;
extern struct pt glossy_pt;


#define INTERNAL_SET_GLOSSY_TX(_max_len, _N, _L, _buffer, _data_len)           \
  do {                                                                         \
    glossy_next_action.is_rx = false;                                          \
    glossy_next_action.update_tref = false;                                    \
    glossy_next_action.max_len = _max_len;                                     \
    glossy_next_action.buffer = _buffer;                                       \
    glossy_next_action.data_len = _data_len;                                   \
    glossy_next_action.N = _N;                                                 \
	glossy_next_action.slivers_per_action = _L;                                \
  } while (0);

#define INTERNAL_SET_GLOSSY_RX(_max_len, _N, _L, _buffer)                      \
  do {                                                                         \
    glossy_next_action.is_rx = true;                                           \
    glossy_next_action.update_tref = false;                                    \
    glossy_next_action.max_len = _max_len;                                     \
    glossy_next_action.buffer = _buffer;                                       \
    glossy_next_action.data_len = 0;                                           \
    glossy_next_action.N = _N;                                                 \
	glossy_next_action.slivers_per_action = _L;                                \
  } while (0);

#define GLOSSY_TX(_pt, _max_len, _N, _buffer, _data_len)                       \
  do {                                                                         \
    INTERNAL_SET_GLOSSY_TX(_max_len,                                           \
			               _N,                                                 \
						   glossy_next_action.slivers_per_action,              \
						   _buffer,                                            \
						   _data_len);                                         \
    PT_SPAWN(_pt, &glossy_pt, glossy_trx());                                   \
  } while (0);

#define GLOSSY_RX(_pt, _max_len, _N, _buffer)                                  \
  do {                                                                         \
	INTERNAL_SET_GLOSSY_RX(_max_len,                                           \
			               _N,                                                 \
						   glossy_next_action.slivers_per_action,              \
						   _buffer);                                           \
    PT_SPAWN(_pt, &glossy_pt, glossy_trx());                                   \
  } while (0);

char glossy_trx();

void glossy_init();

struct glossy_cfg_t {
  uint8_t N;  // number of transmissions in the Glossy round
  uint32_t W; // maximum number of actions in the Glossy round  
  uint8_t L;  // number of slivers used per action in the Glossy round
};

#define GLOSSY_FWD (0)
#define GLOSSY_INIT (1)

#define SAMU_GLOSSY(_pt, _role, _cfg, _b, _txlen)                              \
  do {                                                                         \
    if (_role == GLOSSY_INIT) {                                                \
	  INTERNAL_SET_GLOSSY_TX(_cfg.W, _cfg.N, _cfg.L, _b, _txlen);              \
    } else {                                                                   \
	  INTERNAL_SET_GLOSSY_RX(_cfg.W, _cfg.N, _cfg.L, _b);                      \
    }                                                                          \
    PT_SPAWN(_pt, &glossy_pt, glossy_trx());                                   \
  } while (0);

#ifndef SINK_RADIUS
#error                                                                         \
    "The maximum expected distance from the sink (SINK_RADIUS) must be set (usually in a project-conf.h)"
#endif

#endif
