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
#include "glossy_samu.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>

#include "samu-ral-status.h"
#include "samu.h"

#include "lib/random.h"

#include "dw1000-conv.h"
#include "print-def.h"

#define LOG_PREFIX "gt"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#pragma message STRDEF(GLOSSY_LATENCY_LOG)
#pragma message STRDEF(GLOSSY_MAX_JITTER_MULT)
#pragma message STRDEF(GLOSSY_JITTER_STEP)

#pragma message STRDEF(SAMU_DEFAULT_RXGUARD)
#pragma message STRDEF(SAMU_DEFAULT_SLIVERS_PER_ACTION)

struct glossy_context_t glossy_context;
struct pt glossy_pt;
struct glossy_next_action_t glossy_next_action;

static void glossy_next_action_reset() {
  glossy_next_action = (struct glossy_next_action_t){
      .buf= NULL,
      .data_len = 0,
	  .role = GLOSSY_FWD,
	  .cfg = {
		  .N = 2,
		  .W = 0
	  }
  };
}

void glossy_init() { glossy_next_action_reset(); }

// TODO: To remove?
// void reset_context() {
//   glossy_context.rx_status = GLOSSY_RX_UNINITIALIZED;
//   glossy_context.received_len = 0;
//   glossy_samu_sliver_last_rx = 0;
// }

char glossy_trx();

struct glossy_next_action_t glossy_next_action;

char glossy_trx() {

  PT_BEGIN(&glossy_pt);

  // TODO: To remove?
  // reset_context();

  // NOTE: We do not modify progress_slots as this is part of the interface to
  // interact with glossy_samu

  glossy_context.n_tx           = 0;
  glossy_context.n_actions      = 0;
  glossy_context.forward_packet = 0;

  if (glossy_next_action.role == GLOSSY_FWD) {
    // Try to receive something until the next slot is after at or past deadline
    while (glossy_context.n_actions < glossy_next_action.cfg.W) {
	  SAMU_RX(&glossy_pt, glossy_next_action.buf);
	  glossy_context.n_actions++;
	  if (SAMU_PREV_A.status == RX_SUCCESS) {
		  glossy_context.forward_packet = 1;
		  SAMU_NEXT_A.accept_sync = 1;
		  glossy_next_action.data_len = SAMU_PREV_A.payload_len;
		  break;
	  }
    }
  }

  if ((glossy_next_action.role == GLOSSY_INIT) || (glossy_context.forward_packet)) {
	while((glossy_context.n_actions < glossy_next_action.cfg.W) && 
	      (glossy_context.n_tx < glossy_next_action.cfg.N)) {
	  SAMU_TX(&glossy_pt, glossy_next_action.buf, glossy_next_action.data_len);
	  glossy_context.n_actions++;
	  glossy_context.n_tx++;
	}
  }

  SAMU_SKIP(glossy_next_action.cfg.W - glossy_context.n_actions);

  // TODO: To remove?
  // glossy_next_action_reset();

  PT_END(&glossy_pt);
}
