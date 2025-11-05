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
 * \file      Synchronized Action Manager for UWB (SAMU) APIs
 *
 * TSM (code) authors:
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 *
 * SAMU and Flick (code) authors:
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */

#ifndef SAMU_H
#define SAMU_H

#include "samu-ral.h"
#include <stdbool.h>
#include <stdint.h>

enum samu_action {
  SAMU_ACTION_NONE,    // no action was performed
  SAMU_ACTION_TX,      // slotted TX action
  SAMU_ACTION_RX,      // slotted RX action
  SAMU_ACTION_SCAN,    // start scanning right away till any reception
                       // or a reception error
  SAMU_ACTION_RESTART, // restart the slot series from 0 after a specified
                       // interval
  SAMU_ACTION_STOP,    // request SAMU to stop

  SAMU_ACTION_EVENT,   // slotted event TX
  SAMU_ACTION_EVENT_FP // slotted fast propagation
};

typedef enum samu_ral_status samu_status_t; // SAMU action status

/* Interface structure reporting the status of the action performed by SAMU
 * in the previous slot */
struct samu_prev_action {
  enum samu_action action;     // action performed
  samu_status_t status;        // RAL status of the performed operation
  radio_status_t radio_status; // radio status for the performed operation
  uint8_t *buffer;             // TX/RX buffer
  uint8_t payload_len;         // TX/RX payload length
  uint32_t remote_sliver_idx;  // slot index received in he packet
  int32_t sliver_idx;          // Mini-slot index according to the local counter

  union { // current logic slot index (number of operations)
    __attribute__((deprecated(
        "This is the field name used before the introduction"
        " of the sliver feature, check this code is still working"))) int32_t
        slot_idx;
    int32_t logic_slot_idx;
  };

  union { // current logic slot index (number of operations)
    __attribute__((deprecated(
        "This is the field name used before the introduction"
        " of the sliver feature, check this code is still working"))) uint32_t
        remote_slot_idx;
    uint32_t remote_logic_slot_idx;
  };

  uint32_t progress_slivers; // defines an offset in number of slots when the
                             // action should be performed 1:   go to the next
                             // sliver, N>1: move ahead by N-1 slivers, TODO:
                             // Check default: SAMU_DEFAULT_SLIVERS_PER_ACTION

  uint32_t slivers_to_use; // Number of slivers to schedule for the operation
                           // default: SAMU_DEFAULT_SLIVERS_PER_ACTION
};

/* Inrerface structure requesting the next action for SAMU to perform.
 *
 * The higher layer is expected to fill in the action field as well as
 * other fields which set depends on the requested action.
 * Most of the fields are filled in with reasonable defaults by SAMU and can be
 * kept untouched.
 */
struct samu_next_action {
  enum samu_action action; // action to perform

  union { // defines an offset in number of slots when the action should
          // __logically__ (not physically) be performed 0:   stay within this
          // slot (not implemented) [only RX], 1:   go to the next slot [RX/TX],
          // N>1: skip N-1 slots [RX/TX],
          // default: 1
    __attribute__((deprecated(
        "This is the field name used before the introduction"
        " of the sliver feature, check this code is still working"))) uint32_t
        progress_slots;

    uint32_t progress_logic_slots;
  };

  uint32_t progress_slivers; // defines an offset in number of slots when the
                             // action should be performed 1:   go to the next
                             // sliver, N>1: move ahead by N-1 slivers

  bool accept_sync; // resynch with the packet received in the previous slot
                    // (accept slot id and time reference), meaningful only
                    // after RX slot, ignored if no packet was received.
                    // default: 0

  uint32_t tx_delay; // delay packet transmision w.r.t. the slot reference time,
                     // only meaningful for TX slots
                     // default: 0

  uint32_t restart_interval; // interval to restart
                             // only meaningful for RESTART action, must be set
                             // explicitly
  uint32_t restart_guard;    // duration of restart guard
                             // only meaningful for RESTART action, must be set
                             // explicitly

  union {
    __attribute__((deprecated(
        "This field was renamed. Please use rx_guard_time_before."))) uint32_t
        rx_guard_time; // wake up a bit earlier to compensate for potential
                       // clock drift only meaningful for RX slots
    uint32_t rx_guard_time_before; // default: SAMU_DEFAULT_RXGUARD
  };

  uint32_t rx_guard_time_after;

  uint32_t rx_timeout;

  uint8_t *buffer; // TX/RX buffer
                   // must be set for TX, RX and SCAN operations

  uint8_t payload_len; // TX payload length
                       // must be set for TX slots

  uint32_t slivers_to_use; // Number of slivers to schedule for the operation
                           // default: SAMU_DEFAULT_SLIVERS_PER_ACTION

  uint32_t max_fs_flood_duration; // NOTE: Only used for EVENT

  bool drift_est_enable;

  struct sniff_conf_t sniff;
};

/* Publicly accessible global interface structures used to exchange the
 * information about the previous and the next slot in the slot handler */
extern struct samu_prev_action samu_prev_action;
extern struct samu_next_action samu_next_action;

/* Header of the SAMU layer */
struct samu_header {
  uint16_t tx_delay; // TODO: make it 8-ns based
  uint32_t sliver_idx;
  uint8_t crc;
} __attribute__((packed));

#define SAMU_HDR_LEN sizeof(struct samu_header)

/* Offset of the SAMU header in a packet buffer */
#define SAMU_HDR_OFFS SAMU_RALD_PLD_OFFS

/* Offset of the SAMU payload in a packet buffer */
#define SAMU_PLD_OFFS (SAMU_HDR_OFFS + sizeof(struct samu_header))

/* Slot callback type */
typedef char (*samu_hl_cb)();
typedef uint32_t (*sliver_to_slot_cb)(uint32_t sliver_idx);

struct samu_context_t {
  uint32_t tref; // reference time of the current slot series
                 // (reference time of sliver 0)

  uint32_t slot_duration; // fixed slot duration

  uint32_t slot_rx_timeout; // for RX slots

  uint32_t default_slot_rx_timeout; // the default RX timeout to use

  samu_hl_cb cb; // higher-layer to be resumed

  int32_t sliver_idx; // current sliver index

  uint32_t slivers_to_use; // number of slivers to use in the next action

  sliver_to_slot_cb ms_to_ls_cb; // Upper-layer-defined function to convert from
                                 // sliver to logic slots

  int32_t logic_slot_idx; // current logic slot index; while slivers track time,
                          // logic slots track the number of operations.
                          // Using logic slots in protocol implementations is
                          // entirely optional

  enum samu_action slot_action; // current slot action

  enum samu_ral_status slot_status; // RAL operation status for the current slot

  uint32_t
      tentative_slot_tref; // slot reference of the received packet (if any)
  uint32_t tentative_tref; // tref reference of the received packet (if any)

  uint32_t tentative_sliver_idx; // slot index of the received packet (if any)
  uint32_t
      tentative_logic_slot_idx; // slot index of the received packet (if any)

  uint32_t default_preambleto;

  uint32_t current_restart_interval; // duration of each epoch

  uint32_t current_restart_guard; // duration of each epoch guard (additional to
                                  // defines)

  bool drift_est_enabled; // specifies whether the compensation for drift should
                          // be used

  bool synchronised_epoch; // This epoch was synchronised at least once

  uint32_t first_tref; // The first tref obtained

  struct sniff_conf_t sniff; // Last value of the sniff configuration
};

//--------------------------------------------------------------------------
//   DEFAULT TIMINGS
//--------------------------------------------------------------------------

#ifndef SAMU_DEFAULT_SLIVERS_PER_ACTION
#define SAMU_DEFAULT_SLIVERS_PER_ACTION 1
#endif

#ifndef SAMU_PRE_EPOCH_PROCEDURE_GUARD_US
#define SAMU_PRE_EPOCH_PROCEDURE_GUARD_US 7000
#endif

// NOTE: the defaults below should not be here (since it's DW1000-based)

#ifndef PRE_EPOCH_DURATION
//                            v- maximum rx_guard + preamble length (default,
//                            considers 1000us of guard and 64plen)
#define PRE_EPOCH_DURATION ((1000 + 80 + 100) * UUS_TO_DWT_TIME_32)
//            time for 64plen guard -^    ^- additional time for timer (default)
#endif

#ifndef EPOCH_INIT_DURATION
#define EPOCH_INIT_DURATION ((2000) * UUS_TO_DWT_TIME_32)
//                            ^- time to initialize protocol (default)
#endif

#ifdef SAMU_CONF_DEFAULT_RXGUARD
#define SAMU_DEFAULT_RXGUARD SAMU_CONF_DEFAULT_RXGUARD
#else
#define SAMU_DEFAULT_RXGUARD (10 * UUS_TO_DWT_TIME_32) // receivers guard time
#endif

//--------------------------------------------------------------------------
//   INITIALIZATION
//--------------------------------------------------------------------------

/* Initialise SAMU and all sublayers. Call once on boot */
void samu_init();

/* Specify a default preamble timeout (in radio ticks) */
void samu_set_default_preambleto(const uint32_t preambleto);

/* Start SAMU */
int samu_start(uint32_t slot_duration, uint32_t rx_timeout,
               samu_hl_cb callback);
int samu_sliver_start(uint32_t slot_duration, uint32_t rx_timeout,
                      samu_hl_cb callback,
                      sliver_to_slot_cb sliver_to_slot_converter);

//--------------------------------------------------------------------------
//   HIGHER LAYERS SUPPORT
//--------------------------------------------------------------------------

/* Allow other layers to have some base information about the samu_context_t */
uint32_t samu_get_default_preambleto();
uint32_t samu_get_default_rx_timeout();

/* Allow other layers to set the tref
 * (e.g., useful if you have a way to estimate the UWB sink to node drift) */
void samu_set_tref(uint32_t tref);

//--------------------------------------------------------------------------
//   DIAGNOSTICS SUPPORT
//--------------------------------------------------------------------------

/* Get radio-specific diagnostics */
struct radio_diagnostic_req;
struct radio_diagnostic;
struct radio_diagnostic_req samu_default_radio_diagnostic_req();
struct radio_diagnostic
samu_get_radio_diagnostic(struct radio_diagnostic_req req);

//--------------------------------------------------------------------------
//   TIMING SUPPORT
//--------------------------------------------------------------------------

/* Utilities for 4ns-based timings */
uint32_t samu_get_slot_start_4ns(uint16_t slot_idx);
uint32_t samu_get_slot_end_4ns(uint16_t slot_idx);

//--------------------------------------------------------------------------
//   MAIN APIs
//--------------------------------------------------------------------------

// --- Core Structures ---

#define SAMU_PREV_A samu_prev_action
#define SAMU_NEXT_A samu_next_action

// --- Slivers (Time Management) ---

#define SAMU_SET_SLIVERS_PER_ACTION(a)                                         \
  do {                                                                         \
    SAMU_NEXT_A.progress_slivers += a - SAMU_DEFAULT_SLIVERS_PER_ACTION;       \
    SAMU_NEXT_A.slivers_to_use = a;                                            \
  } while (0)
#define SAMU_PREV_SLIVER_IDX SAMU_PREV_A.sliver_idx
#define SAMU_NEXT_SLIVER_IDX                                                   \
  (SAMU_PREV_A.sliver_idx + SAMU_NEXT_A.progress_slivers -                     \
   SAMU_DEFAULT_SLIVERS_PER_ACTION + SAMU_NEXT_A.slivers_to_use) //??
#define SAMU_SKIP(n)                                                           \
  do {                                                                         \
    SAMU_NEXT_A.progress_slivers += (n);                                       \
  } while (0)

// --- Actions (Radio Operations) ---

// TX (single transmission)
// Supports SAMU_NEXT_A.tx_delay
#define SAMU_TX(p, b, l)                                                       \
  do {                                                                         \
    SAMU_NEXT_A.action = SAMU_ACTION_TX;                                       \
    SAMU_NEXT_A.buffer = b;                                                    \
    SAMU_NEXT_A.payload_len = l;                                               \
    PT_YIELD(p);                                                               \
  } while (0)

// RX (single reception)
// Supports SAMU_NEXT_A.rx_guard_time_before, SAMU_NEXT_A.rx_guard_time_after
// After a successful SAMU_RX reception, SAMU_NEXT_A.accept_sync can be used to
// synchronize
#define SAMU_RX(p, b)                                                          \
  do {                                                                         \
    SAMU_NEXT_A.action = SAMU_ACTION_RX;                                       \
    SAMU_NEXT_A.buffer = b;                                                    \
    PT_YIELD(p);                                                               \
  } while (0)

// SCAN (continuous RX)
// Supports SAMU_NEXT_A.rx_guard_time_before, SAMU_NEXT_A.sniff
// After a successful SAMU_SCAN reception, SAMU_NEXT_A.accept_sync can be used
// to synchronize
#define SAMU_SCAN(p, b)                                                        \
  do {                                                                         \
    SAMU_NEXT_A.action = SAMU_ACTION_SCAN;                                     \
    SAMU_NEXT_A.buffer = b;                                                    \
    PT_YIELD(p);                                                               \
  } while (0)

// FLICK (preamble transmission and detection for binary value dissemination)
#define FLICK_FWD (0)
#define FLICK_INIT (1)
#define SAMU_FLICK(p, r)                                                       \
  do {                                                                         \
    SAMU_NEXT_A.action = ((r) ? SAMU_ACTION_EVENT : SAMU_ACTION_EVENT_FP);     \
    PT_YIELD(p);                                                               \
  } while (0)

// --- Utility ---

/* Starts SAMU, waking up the radio and setting a local time reference */
#define SAMU_START(p, s, r) samu_start(s, r, p)

/* Restart from slot 0 after the specified interval (in us) w.r.t. to the last
 * slot 0 */
#define SAMU_RESTART(p, e)                                                     \
  do {                                                                         \
    SAMU_NEXT_A.action = SAMU_ACTION_RESTART;                                  \
    SAMU_NEXT_A.restart_interval = e;                                          \
    SAMU_NEXT_A.restart_guard = 0;                                             \
    PT_YIELD(p);                                                               \
  } while (0)

/* Restart from slot 0 after the specified interval (in us) w.r.t. to the last
 * slot 0 */
#define SAMU_RESTART_GUARDED(p, e, g)                                          \
  do {                                                                         \
    SAMU_NEXT_A.action = SAMU_ACTION_RESTART;                                  \
    SAMU_NEXT_A.restart_interval = e;                                          \
    SAMU_NEXT_A.restart_guard = g;                                             \
    PT_YIELD(p);                                                               \
  } while (0)

/* Diagnostics */
#if SAMU_SUPPORT_CIR
#define SAMU_GET_DIAGNOSTICS(d)                                                \
  do {                                                                         \
    d = samu_get_radio_diagnostic(                                             \
        rxTimestamp = true, txTimestamp = true, fpIdx = true, noise = true,    \
        firstPathAmplitudes = true, maxGrowthCIR = true, rxPreamCounts = true, \
        peakPath = true, start_CIR = 0, length_CIR = MAX_CIR_SIZE);            \
  } while (0)
#else
#define SAMU_GET_DIAGNOSTICS(d)                                                \
  do {                                                                         \
    d = samu_get_radio_diagnostic(                                             \
        rxTimestamp = true, txTimestamp = true, fpIdx = true, noise = true,    \
        firstPathAmplitudes = true, maxGrowthCIR = true, rxPreamCounts = true, \
        peakPath = true);                                                      \
  } while (0)
#endif

#endif // SAMU_H
