/*
 * Copyright (c) 2020, University of Trento.
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
 * \file      Time Slot Manager (TSM) API
 *
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 */

#ifndef TREX_TSM_H
#define TREX_TSM_H

#include <stdint.h>
#include <stdbool.h>
#include "trex-driver.h"

enum tsm_action {
  TSM_ACTION_NONE,      // no action was performed
  TSM_ACTION_TX,        // slotted TX action
  TSM_ACTION_RX,        // slotted RX action
  TSM_ACTION_SCAN,      // start scanning right away till any reception 
                        // or a reception error
  TSM_ACTION_RESTART,   // restart the slot series from 0 after a specified 
                        // interval
  TSM_ACTION_STOP,      // request TSM to stop

#if TARGET == evb1000
  TSM_ACTION_EVENT,     // slotted event TX
  TSM_ACTION_EVENT_FP   // slotted fast propagation
#endif
};

typedef enum trex_status tsm_status_t; // TSM action status

/* Interface structure reporting the status of the action performed by TSM 
 * in the previous slot */
struct tsm_prev_action {
    enum tsm_action action;            // action performed
    tsm_status_t    status;            // Trex status of the performed operation
    radio_status_t  radio_status;      // radio status for the performed operation
    //uint32_t        slot_ref_time;     // SFD time of the past TX or RX (if any) minus the TX delay (TODO)
    uint8_t*        buffer;            // TX/RX buffer
    uint8_t         payload_len;       // TX/RX payload length

    uint32_t       remote_minislot_idx;   // slot index received in the packet
    int32_t        minislot_idx;      // Mini-slot index according to the local counter

    union {                            // current logic slot index (number of operations)
      __attribute__((deprecated("This is the field name used before the introduction"
                                " of the minislot feature, check this code is still working")))
      int32_t        slot_idx;
      int32_t        logic_slot_idx;
    };

    union {                            // current logic slot index (number of operations)
      __attribute__((deprecated("This is the field name used before the introduction"
                                " of the minislot feature, check this code is still working")))
      uint32_t        remote_slot_idx;
      uint32_t        remote_logic_slot_idx;
    };
};

/* Inrerface structure requesting the next action for TSM to perform.
 *
 * The higher layer is expected to fill in the action field as well as
 * other fields which set depends on the requested action.
 * Most of the fields are filled in with reasonable defaults by TSM and can be 
 * kept untouched.
 */
struct tsm_next_action {
    enum tsm_action action;     // action to perform

    //uint32_t slot_ref_time;     // expected SFD time for the next slot (assuming TX delay 0)
    union {                     // defines an offset in number of slots when the action should __logically__ (not physically) be performed
                                // 0:   stay within this slot (not implemented) [only RX],
                                // 1:   go to the next slot [RX/TX],
                                // N>1: skip N-1 slots [RX/TX],
                                // default: 1
        __attribute__((deprecated("This is the field name used before the introduction"
                                  " of the minislot feature, check this code is still working")))
        uint32_t progress_slots;

        uint32_t progress_logic_slots;
    };

    uint32_t progress_minislots;// defines an offset in number of slots when the action should be performed
                                // 0:   stay within this slot (not implemented) [only RX],
                                // 1:   go to the next slot [RX/TX],
                                // N>1: skip N-1 slots [RX/TX],
                                // default: 1

    bool     accept_sync;       // resynch with the packet received in the previous slot (accept slot id 
                                // and time reference), meaningful only after RX slot,
                                // ignored if no packet was received
                                // default: 0

    uint32_t tx_delay;          // delay packet transmision w.r.t. the slot reference time,
                                // only meaningful for TX slots
                                // default: 0

    uint32_t restart_interval;  // interval to restart
                                // only meaningful for RESTART action, must be set explicitly

    uint32_t rx_guard_time;     // wake up a bit earlier to compensate for potential clock drift
                                // only meaningful for RX slots
                                // default: TSM_DEFAULT_RXGUARD

    uint8_t* buffer;            // TX/RX buffer
                                // must be set for TX, RX and SCAN operations

    uint8_t  payload_len;       // TX payload length
                                // must be set for TX slots

    uint32_t minislots_to_use;  // Number of minislots to schedule for the operation
                                // default: TSM_DEFAULT_MINISLOTS_ADVANCE
#if TARGET == evb1000
    uint32_t max_fs_flood_duration; // NOTE: Only used for EVENT
#endif
};

/* Publicly accessible global interface structures used to exchange the
 * information about the previous and the next slot in the slot handler */
extern struct tsm_prev_action tsm_prev_action;
extern struct tsm_next_action tsm_next_action;

/* Header of the TSM layer */
struct tsm_header {
  uint16_t tx_delay; // TODO: make it 8-ns based
  uint32_t minislot_idx;
  uint8_t crc;
} __attribute__((packed));

#define TSM_HDR_LEN sizeof(struct tsm_header)

/* Offset of the TSM header in a packet buffer */
#define TSM_HDR_OFFS TREXD_PLD_OFFS

/* Offset of the TSM payload in a packet buffer */
#define TSM_PLD_OFFS (TSM_HDR_OFFS + sizeof(struct tsm_header))

/* Slot callback type */
typedef void (*tsm_slot_cb)();
typedef uint32_t (*minislot_to_slot_cb)(uint32_t minislot_idx);

#if TARGET == evb1000
#define TSM_FS_ACTION(pt, fs_action, fs_macroslot_duration, fs_minislot_duration) do {tsm_next_action.progress_logic_slots += fs_macroslot_duration-1;\
                                                                                      tsm_next_action.progress_minislots   += fs_minislot_duration-TSM_DEFAULT_MINISLOTS_GROUPING;\
                                                                                      tsm_next_action.minislots_to_use      = fs_minislot_duration;\
                                                                                      tsm_next_action.action = fs_action;\
                                                                                      PT_YIELD(pt);} while (0)

/* Schedule Flick TX in the next slot and yield the protothread */
#define TSM_TX_FS_SLOT(pt, macroslot_duration, minislot_duration) do{TSM_FS_ACTION(pt, TSM_ACTION_EVENT, macroslot_duration, minislot_duration);} while(0)

/* Schedule Flick TX and repropagation in the next slot and yield the protothread */
#define TSM_RX_FS_SLOT(pt, macroslot_duration, minislot_duration) do{TSM_FS_ACTION(pt, TSM_ACTION_EVENT_FP, macroslot_duration, minislot_duration);} while(0)
#endif

/* Schedule TX in the next slot and yield the protothread */
#define TSM_TX_SLOT(pt, _buffer, len) do {tsm_next_action.action=TSM_ACTION_TX;\
                                          tsm_next_action.buffer=_buffer;\
                                          tsm_next_action.payload_len=len;\
                                          PT_YIELD(pt);} while(0)

/* Schedule RX in the next slot and yield the protothread */
#define TSM_RX_SLOT(pt, _buffer) do {tsm_next_action.action=TSM_ACTION_RX;\
                                     tsm_next_action.buffer=_buffer;\
                                     PT_YIELD(pt);} while(0)

/* Start scanning (listening) immediately and yield the protothread */
#define TSM_SCAN(pt, _buffer) do {tsm_next_action.action=TSM_ACTION_SCAN;\
                                  tsm_next_action.buffer=_buffer;\
                                  PT_YIELD(pt);} while(0)



/* Restart from slot 0 after the specified interval w.r.t. to the last slot 0 */
#define TSM_RESTART(pt, interval) do{tsm_next_action.action=TSM_ACTION_RESTART;\
                                        tsm_next_action.restart_interval=interval;\
                                        PT_YIELD(pt);} while(0)
// TODO: let it reconfigure the slot duration and the rx timeout on restart?

/* Initialise TSM and all sublayers. Call once on boot. */
void tsm_init();
void tsm_set_default_preambleto(const uint32_t preambleto);
uint32_t tsm_get_default_preambleto();

/* Start TSM */
int tsm_start(uint32_t slot_duration, uint32_t rx_timeout, tsm_slot_cb callback);
int tsm_minislot_start(uint32_t slot_duration, uint32_t rx_timeout, tsm_slot_cb callback, minislot_to_slot_cb minislot_to_slot_converter);

struct tsm_context_t {
  uint32_t         tref;                          // reference time of the current slot series (reference time of slot 0)
  uint32_t         slot_duration;                 // fixed slot duration
  uint32_t         slot_rx_timeout;               // for RX slots
  tsm_slot_cb      cb;                            // higher-layer callback (called between slots)

  int32_t          minislot_idx;                  // current minislot index
  uint32_t         minislots_to_use;              // number of minislots to use in the next action
  minislot_to_slot_cb ms_to_ls_cb;                // Upper-layer-defined function to convert from minislot to logic slots

  union {                                         // current logic slot index (number of operations)
    __attribute__((deprecated("This is the field name used before the introduction"
                              " of the minislot feature, check this code is still working")))
    int32_t        slot_idx;
    int32_t        logic_slot_idx;
  };

  enum tsm_action  slot_action;                   // current slot action
  //uint32_t         slot_tref;        // current slot reference time (TODO)
  enum trex_status slot_status;                   // Trex operation status for the current slot

  uint32_t         tentative_slot_tref;           // slot reference of the received packet (if any)
  uint32_t         tentative_tref;                // tref reference of the received packet (if any)

  uint32_t         tentative_minislot_idx;        // slot index of the received packet (if any)
  uint32_t         tentative_logic_slot_idx;      // slot index of the received packet (if any)

  uint32_t         default_preambleto;
  uint16_t         default_preambleto_pacs;       // cache the default timeout in PACs units
  uint32_t         default_preambleto_actual_4ns; // Actual duration of the preambleto in 4ns

  bool             epoch_initialized;             // true if the timer-mapping was already initialized for this epoch or not
};

#if TARGET == evb1000
uint32_t tsm_get_slot_start_4ns(uint16_t slot_idx);
uint32_t tsm_get_slot_end_4ns(uint16_t slot_idx);
#endif

#ifndef TSM_DEFAULT_MINISLOTS_GROUPING
#define TSM_DEFAULT_MINISLOTS_GROUPING 1
#endif

#ifndef PRE_EPOCH_DURATION
//                            v- maximum rx_guard + preamble length (default, considers 1000us of guard and 64plen)
#define PRE_EPOCH_DURATION ((1000 + 80 + 100)* UUS_TO_DWT_TIME_32)
//            time for 64plen guard -^    ^- additional time for timer (default)
#endif

#ifndef EPOCH_INIT_DURATION
#define EPOCH_INIT_DURATION ((2000)* UUS_TO_DWT_TIME_32)
//                            ^- time to initialize protocol (default)
#endif

#endif // TREX_TSM_H
