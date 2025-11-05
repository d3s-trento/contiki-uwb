/*
 * Copyright (c) 2025, University of Trento.
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
/*
 * \file      Synchronized Action Manager for UWB (SAMU) action logging APIs
 *
 * TSM (code) authors: (initially in trex-tsm.c)
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 *
 * SAMU and Flick (code) authors:
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */

#include "samu-logs.h"

#include <stdio.h>
#include <string.h>

#include "print-def.h"

#include "ascii85.h"

#define LOG_PREFIX "samul"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

enum samu_log_status {
  SAMU_LOG_RX_SUCCESS = 0,
  SAMU_LOG_RX_TIMEOUT = 1,
  SAMU_LOG_RX_ERROR = 2,
  SAMU_LOG_RX_MALFORMED = 3,
  SAMU_LOG_TIMER_EVENT = 4,
  SAMU_LOG_TX_DONE = 5,
  SAMU_LOG_FLICK_FALSE = 6,
  SAMU_LOG_FLICK_DETECTED = 7,
  SAMU_LOG_FLICK_TRUE = 8,
  SAMU_LOG_FLICK_ERROR = 9,
  SAMU_LOG_RX_WITH_SYNCH = 10,
  SAMU_LOG_SCAN_WITH_SYNCH = 11,
  SAMU_LOG_UNKNOWN = 12,
};

struct samu_log_t { // 9 bytes 5 bits
  // uint8_t m_hdr:1; // Added automatically
  enum samu_log_status status : 4;

  uint8_t hdr;
  uint32_t sliver : 24;
  int16_t sliver_idx_diff;

  uint8_t slivers_to_use;
  uint16_t progress_slivers;
};

struct bitbuf_state_t {
  uint8_t *dest_buf;
  uint8_t offset;
};

#ifndef SAMU_LOGS_MAX
#define SAMU_LOGS_MAX 192
#endif

#pragma message STRDEF(SAMU_LOGS_MAX)

static struct {
  uint8_t log_str[SAMU_LOGS_MAX + 5];
  uint16_t logged_slots;
  uint16_t nslots;
  struct bitbuf_state_t bitbuf_state;
} samu_log_state;

static inline enum samu_log_status convert_status(enum samu_ral_status status,
                                                  enum samu_action action,
                                                  bool accept_sync) {
  switch (status) {
  case RX_SUCCESS:
    if (action == SAMU_ACTION_SCAN) {
      if (accept_sync) {
        return SAMU_LOG_SCAN_WITH_SYNCH;
      } else {
        return SAMU_LOG_SCAN_WITH_SYNCH; // TODO: should have a dedicated value
      }
    } else if (accept_sync) {
      return SAMU_LOG_RX_WITH_SYNCH;
    } else {
      return SAMU_LOG_RX_SUCCESS;
    }
  case RX_TIMEOUT:
    return SAMU_LOG_RX_TIMEOUT;
  case RX_ERROR:
    return SAMU_LOG_RX_ERROR;
  case RX_MALFORMED:
    return SAMU_LOG_RX_MALFORMED;
  case TIMER_EVENT:
    return SAMU_LOG_TIMER_EVENT;
  case TX_DONE:
    return SAMU_LOG_TX_DONE;
  case FLICK_FALSE:
    return SAMU_LOG_FLICK_FALSE;
  case FLICK_DETECTED:
    return SAMU_LOG_FLICK_DETECTED;
  case FLICK_TRUE:
    return SAMU_LOG_FLICK_TRUE;
  case FLICK_ERROR:
    return SAMU_LOG_FLICK_ERROR;
  case STATUS_NONE:
    WARN("Tried to log STATUS_NONE");
    return SAMU_LOG_UNKNOWN;
  default:
    WARN("Unknown samu_ral_status value");
    return SAMU_LOG_UNKNOWN;
  }
}

void samu_log_init() {
  memset(samu_log_state.log_str, 0,
         sizeof(samu_log_state.log_str) / sizeof(uint8_t));
  samu_log_state.logged_slots = 0;
  samu_log_state.nslots = 0;

  samu_log_state.bitbuf_state.dest_buf = samu_log_state.log_str + 5;
  samu_log_state.bitbuf_state.offset = 0;
}

static inline struct bitbuf_state_t
add_on_offset(uint8_t value, uint8_t bits, struct bitbuf_state_t dest_buf) {
  dest_buf.dest_buf[0] |= (value << (8 - bits)) >> dest_buf.offset;

  if (bits + dest_buf.offset >= 8) {
    dest_buf.dest_buf[1] |= (value << (8 - bits)) << (8 - dest_buf.offset);
    dest_buf.dest_buf += 1;
  }

  dest_buf.offset = (bits + dest_buf.offset) % 8;

  return dest_buf;
}

#define MASK_SLIVERS 1
#define MASK_SLIVERS_IDX_DIFF (1 << 1)
#define MASK_SLIVERS_TO_USE (1 << 2)
#define MASK_PROGRESS_SLIVERS (1 << 3)

static inline struct bitbuf_state_t add_value(const struct samu_log_t val,
                                              struct bitbuf_state_t dest_buf) {
  dest_buf = add_on_offset(val.hdr != 0, 1, dest_buf);
  dest_buf = add_on_offset(val.status & 0x0f, 4, dest_buf);

  if (val.hdr != 0) {
    dest_buf = add_on_offset(val.hdr, 8, dest_buf);
  }

  if (val.hdr & MASK_SLIVERS) {
    dest_buf = add_on_offset((val.sliver >> 16) & 0xff, 8, dest_buf);
    dest_buf = add_on_offset((val.sliver >> 8) & 0xff, 8, dest_buf);
    dest_buf = add_on_offset((val.sliver) & 0xff, 8, dest_buf);
  }

  if (val.hdr & MASK_SLIVERS_IDX_DIFF) {
    dest_buf = add_on_offset((val.sliver_idx_diff >> 8) & 0xff, 8, dest_buf);
    dest_buf = add_on_offset((val.sliver_idx_diff) & 0xff, 8, dest_buf);
  }

  if (val.hdr & MASK_SLIVERS_TO_USE) {
    dest_buf = add_on_offset(val.slivers_to_use, 8, dest_buf);
  }

  if (val.hdr & MASK_PROGRESS_SLIVERS) {
    dest_buf = add_on_offset((val.progress_slivers >> 8) & 0xff, 8, dest_buf);
    dest_buf = add_on_offset((val.progress_slivers) & 0xff, 8, dest_buf);
  }

  return dest_buf;
}

void samu_log_append(enum samu_ral_status status, enum samu_action action,
                     bool accept_sync, uint32_t sliver_idx,
                     uint8_t slivers_to_use, uint16_t progress_slivers,
                     int16_t sliver_idx_diff) {
  ++samu_log_state.nslots;
  if (samu_log_state.nslots == 0) {
    ERR("samu_log_state.logged_slots overflow");
    return;
  }

  // The maximum size of a single append should be 9 bytes, 5 bits
  // Thus as the maximum offset is 7 it should be at max 10 bytes, 6 bit from
  // the first bit of the dest_buf Rounding to 10.<something> to 11 > instead of
  // >= to add one extra byte of tolerance
  if (!(samu_log_state.log_str +
            sizeof(samu_log_state.log_str) / sizeof(uint8_t) -
            samu_log_state.bitbuf_state.dest_buf >
        11)) {
    // The slot to append might use more bytes than available, stop here
    return;
  }

  if (samu_log_state.logged_slots + 1 <= SAMU_LOGS_MAX) {
    ++samu_log_state.logged_slots;
  } else {
    // It won't be logged anyway so we might as well stop here
    return;
  }

  struct samu_log_t val = {
      .hdr = 0,
      .status = convert_status(status, action, accept_sync),
      .progress_slivers = progress_slivers,
      .slivers_to_use = slivers_to_use,
      .sliver = sliver_idx,
  };

  if (val.status == SAMU_LOG_RX_WITH_SYNCH ||
      val.status == SAMU_LOG_SCAN_WITH_SYNCH) {
    val.hdr |= MASK_SLIVERS;

    // Do not print the diff between that we expected and what we received as we
    // are synchronizing
    val.sliver_idx_diff = 0;
  }

  if (val.sliver_idx_diff != 0) {
    val.hdr |= MASK_SLIVERS_IDX_DIFF;
  }

  if (val.slivers_to_use != SAMU_DEFAULT_SLIVERS_PER_ACTION) {
    val.hdr |= MASK_SLIVERS_TO_USE;
  }

  if (val.progress_slivers != SAMU_DEFAULT_SLIVERS_PER_ACTION) {
    val.hdr |= MASK_PROGRESS_SLIVERS;
  }

  samu_log_state.bitbuf_state = add_value(val, samu_log_state.bitbuf_state);
}

#undef MASK_SLIVERS
#undef MASK_SLIVERS_IDX_DIFF
#undef MASK_SLIVERS_TO_USE
#undef MASK_PROGRESS_SLIVERS

void samu_log_print() {
  uint8_t out_buf[((sizeof(samu_log_state.log_str) + 3) / 4) * 5 + 1];
  memset(out_buf, 0, sizeof(out_buf) / sizeof(uint8_t));

  samu_log_state.log_str[0] = SAMU_DEFAULT_SLIVERS_PER_ACTION & 0xff;
  samu_log_state.log_str[1] = (samu_log_state.logged_slots >> 8) & 0xff;
  samu_log_state.log_str[2] = samu_log_state.logged_slots & 0xff;
  samu_log_state.log_str[3] = (samu_log_state.nslots >> 8) & 0xff;
  samu_log_state.log_str[4] = samu_log_state.nslots & 0xff;

  ascii85_encode(
      out_buf, sizeof(out_buf) / sizeof(uint8_t), samu_log_state.log_str,
      samu_log_state.bitbuf_state.dest_buf + 1 - samu_log_state.log_str + 1);

  printf("[" LOG_PREFIX " %lu]Slots: %s\n", logging_context, out_buf);

  // Reset the SAMU log
  samu_log_init();
}
