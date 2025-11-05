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
 * \file Synchronized Action Manager for UWB (SAMU) Deep-sleep API for Contiki
 *
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */
#include "contiki-samu.h"

#include "contiki.h"
#include "dev/watchdog.h"

#include "dw1000-conv.h"
#include "rtimer-drift.h"
#include "rtimer.h"
#include "uwb-to-rtimer.h"

#include "samu-ral.h"

#include "print-def.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#define LOG_PREFIX "c-samu"
#define LOG_LEVEL LOG_WARN
#include "logging.h"

#if STATETIME_CONF_ON
#include "dw1000-statetime.h"
#ifndef STATETIME_PRE_EPOCH_DURATION
#define STATETIME_PRE_EPOCH_DURATION (60 * UUS_TO_DWT_TIME_32)
#endif
#else
#ifndef STATETIME_PRE_EPOCH_DURATION
#define STATETIME_PRE_EPOCH_DURATION (60 * UUS_TO_DWT_TIME_32)
#endif
#endif

#define FATAL(...)                                                             \
  do {                                                                         \
    ERR(__VA_ARGS__);                                                          \
    fflush(NULL);                                                              \
    watchdog_reboot();                                                         \
  } while (0);

#ifndef RESTART_LOGS
#define RESTART_LOGS 0
#endif

static int32_t offset_ns = 0;

static rtimer_clock_t start_end;

static rtimer_clock_t start;
static uint64_t next_epoch_start = 0;

static int32_t time_to_wait;

static int32_t time_passed_us;
static uint32_t time_passed_uwb;

static epoch_start_cb cb;
static epoch_start_cb up_layer_cb;
static struct samu_context_t *samu_ctx;

static uint64_t rtick_passed_ext;

static uint32_t estimated_tref = 0;
static uint64_t additional_wakeup_time = 0;

static void contiki_samu_wakeup_and_start_epoch();

PROCESS(restart_epoch_process, "Process that restarts a samu epoch");

void contiki_samu_init(epoch_start_cb es_cb, epoch_start_cb ul_cb,
                       struct samu_context_t *samu_context) {
  rde_init();

  cb = es_cb;
  up_layer_cb = ul_cb;
  samu_ctx = samu_context;

  process_start(&restart_epoch_process, NULL);
}

rtimer_clock_t contiki_samu_us_to_rtick(uint32_t us) {
  return rde_us_to_rtick(us);
}

void contiki_samu_start() {
  rtimer_clock_t epoch_start_rt = RTIMER_NOW();
  uint32_t epoch_start_uwb = samu_rald_now_time();

  if (samu_ctx->drift_est_enabled) {
    utr_start_epoch(epoch_start_uwb, epoch_start_rt);
  }

  samu_ctx->tref =
      epoch_start_uwb + STATETIME_PRE_EPOCH_DURATION + PRE_EPOCH_DURATION;
  samu_ctx->first_tref = samu_ctx->tref;
  estimated_tref = samu_ctx->tref;

  bool res = samu_rald_pre_epoch_procedure(epoch_start_uwb +
                                           STATETIME_PRE_EPOCH_DURATION);

  if (!res) {
    FATAL("Error during the samu-ral pre epoch procedure");
    return;
  }

#if !STATETIME_CONF_ON
  cb();
#endif
}

int contiki_samu_goto_sleep() {

  if (time_passed_us <= 0) {
    ERR("Time passed between the last operation and the start of the "
        "corresponding epoch is < 0");
    return -1;
  }

  time_to_wait =
      samu_ctx->current_restart_interval - samu_ctx->current_restart_guard;

  if (time_to_wait <= 0) {
    ERR("The remaining duration of the passive part of the epoch is < 0");
    return -1;
  }

  samu_rald_sleep();

  if (samu_ctx->synchronised_epoch) {
#if RESTART_LOGS
    printf("[" LOG_PREFIX " %" PRIu32 "] tref diff %" PRIi32
           " additional %" PRIu64 "\n",
           logging_context, (int32_t)(samu_ctx->first_tref - estimated_tref),
           additional_wakeup_time);
#endif
  }

  if (samu_ctx->synchronised_epoch && samu_ctx->drift_est_enabled) {
    rde_add(start, utr_uwb_to_rtick_ext(time_passed_uwb));
  } else {
    rde_skip();
  }

  // Note: we first, if possible, calculate using the restart interval
  // between this epoch and the previous one and only later update the
  // restart interval to use
  rde_set_expected_time(samu_ctx->current_restart_interval);
  samu_ctx->synchronised_epoch = false;

  return 0;
}

void contiki_samu_epoch_restart() {
  {
    static rtimer_clock_t old;
    old = RTIMER_NOW();
    do {
      start = RTIMER_NOW();
    } while (start == old);

    // Check that we didn't skip any timestamp (can we be interrupted by higher
    // priority interrupts?)
    if (old + 1 != start) {
      samu_ctx->synchronised_epoch = false;
    }
  }

  uint32_t now = samu_rald_now_time();
  time_passed_us = samu_rald_time_between_us(now, samu_ctx->first_tref);
  time_passed_uwb = now - samu_ctx->first_tref;

  if (samu_ctx->drift_est_enabled) {
    utr_end_epoch(now, start);
  }

  rtick_passed_ext = utr_uwb_to_rtick_ext(now - samu_ctx->first_tref);

#if RESTART_LOGS
  printf("[" LOG_PREFIX " %" PRIu32 "] rtick passed %" PRIu32 " for %" PRIu32
         " us\n",
         logging_context, (uint32_t)(rtick_passed_ext >> 15), time_passed_us);
#endif

  samu_ctx->drift_est_enabled = SAMU_NEXT_A.drift_est_enable;

  process_poll(&restart_epoch_process);
}

static void contiki_samu_wakeup_and_start_epoch() {
  static uint64_t guards_rtick_ext;
  static rtimer_clock_t this_end;
  static int32_t remaining_time;

  watchdog_periodic();
  if (!samu_rald_wakeup()) {
    FATAL("Radio wakeup failed. Restarting... ");
    return;
  }
  watchdog_periodic();

  guards_rtick_ext = utr_uwb_to_rtick_ext(STATETIME_PRE_EPOCH_DURATION +
                                          PRE_EPOCH_DURATION - offset_ns);

  uint64_t start_end_64 = next_epoch_start - guards_rtick_ext;
  this_end = start_end_64 >> 15;

  additional_wakeup_time =
      ((uint64_t)(start_end_64 & 0x7fff) * ((uint64_t)utr_get_ratio_u32())) >>
      32;

  while (RTIMER_CLOCK_DIFF(this_end, RTIMER_NOW() + 4) >= 0) {
    watchdog_periodic();
  }

  if (!samu_rald_prepare_radio()) {
    FATAL("It was not possible to bring back up the radio. Restarting... ");
    return;
  }

  // From now on do the same thing as the  start
  rtimer_clock_t rt_now;

  {
    static rtimer_clock_t old;
    old = RTIMER_NOW();
    do {
      rt_now = RTIMER_NOW();
    } while (rt_now == old);
  }
  uint32_t epoch_start_uwb_tmp = samu_rald_now_time();

  remaining_time = RTIMER_CLOCK_DIFF(this_end, RTIMER_NOW());
  if (remaining_time <= 0) {
    FATAL("No remaining time to set the restart of the epoch (stage2)");
  }

  // The calculations we did consider the end of the rtick so we wait for the
  // rtick to no longer be the one we calculated
  while (RTIMER_CLOCK_DIFF(this_end, RTIMER_NOW() + 1) >= 0) {
    watchdog_periodic();
  }

  uint32_t epoch_start_uwb = samu_rald_now_time();

  if (samu_ctx->drift_est_enabled) {
    utr_start_epoch(epoch_start_uwb_tmp, rt_now);
  }

  samu_ctx->tref = epoch_start_uwb + STATETIME_PRE_EPOCH_DURATION +
                   PRE_EPOCH_DURATION + additional_wakeup_time;
  samu_ctx->first_tref = samu_ctx->tref;
  estimated_tref = samu_ctx->tref;

  bool res = samu_rald_pre_epoch_procedure(
      epoch_start_uwb + STATETIME_PRE_EPOCH_DURATION + additional_wakeup_time);

  if (!res) {
    FATAL("Error during the samu_rald_pre_epoch_procedure. Restarting... ");
    return;
  }

#if !STATETIME_CONF_ON
  cb();
#endif
}

static void contiki_samu_wait_for_epoch_start(struct rtimer *rt, void *ptr) {
#if STATETIME_CONF_ON
  dw1000_statetime_pre_epoch_total_guard(
      SAMU_PRE_EPOCH_PROCEDURE_GUARD_US * 1000 / DWT_TICK_TO_NS_32 +
      STATETIME_PRE_EPOCH_DURATION + PRE_EPOCH_DURATION - offset_ns);
#endif

  // When we get close to the actual deadline no longer yield time to other
  // processes
  while (RTIMER_CLOCK_DIFF(start_end, RTIMER_NOW()) >= 0) {
    watchdog_periodic();
  }

  contiki_samu_wakeup_and_start_epoch();
}

PROCESS_THREAD(restart_epoch_process, ev, data) {
  static rtimer_clock_t watchdog_end;
  static uint64_t guards_rtick_ext;

  static uint32_t prev_restart_guard;
  static uint32_t current_restart_guard;

  static uint32_t prev_restart_interval;
  static uint32_t current_restart_interval;

  static int was_synchronized = 0;

  static struct rtimer rt;

  PROCESS_BEGIN();

  while (1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_POLL);

    if (samu_ctx->synchronised_epoch && samu_ctx->drift_est_enabled) {
      was_synchronized = was_synchronized >= 5 ? 5 : (was_synchronized + 1);
    } else {
      was_synchronized = 0;
    }

    current_restart_guard = samu_ctx->current_restart_guard;
    current_restart_interval = samu_ctx->current_restart_interval;

    int ret = contiki_samu_goto_sleep();
    if (ret >= 0) {
      up_layer_cb();
    } else {
      FATAL("contiki_samu_goto_sleep had an issue");
    }

    if (SAMU_NEXT_A.action == SAMU_ACTION_SCAN && was_synchronized == 0) {
      offset_ns = 0;
    }

    // If we are exiting an epoch in which we synchronized successfully and we
    // are going to start the next epoch with a reception try to take into
    // account the offset that we got in the just finished epoch
    if (was_synchronized > 1) {
      // In this calculation the clock drift should not influence the result
      // significantly as it should be as prev_restart_guard should be a small
      // value and thus so should be the accumulated clock drift
      int32_t guard_offset = prev_restart_guard * 1000 / DWT_TICK_TO_NS_32;
      int32_t calculated_offset =
          (((int32_t)(samu_ctx->first_tref - estimated_tref)) - guard_offset);

      uint64_t max_allowed =
          (prev_restart_interval *
           ((uint64_t)(THEORETICAL_RDE_CLOCK * (RDE_MAX_PPM * 1e-6))) * 1000 /
           DWT_TICK_TO_NS_32);

      int32_t old_offset_ns = offset_ns;

      if ((((uint64_t)ABS(calculated_offset)) << 25) <= max_allowed) {
        offset_ns += calculated_offset;

#define FILTER_OUT_BOUNDARY_NS 40000

        // If the new value is invalid restore the old one
        if ((offset_ns < 0) && (-offset_ns > FILTER_OUT_BOUNDARY_NS)) {
          WARN("Offset hit boundaries (val: %" PRIi32 ")", offset_ns);
          offset_ns = old_offset_ns;
        } else if ((offset_ns > 0) && (offset_ns > FILTER_OUT_BOUNDARY_NS)) {
          WARN("Offset hit boundaries (val: %" PRIi32 ")", offset_ns);
          offset_ns = old_offset_ns;
        }
      } else {
        WARN("Offset ignored %" PRIu64 " (max %" PRIu64 ")",
             (((uint64_t)ABS(calculated_offset)) << 25), max_allowed);
      }

#if RESTART_LOGS
      printf("[" LOG_PREFIX " %" PRIu32 "]Offset %" PRIi32 "\n",
             logging_context, offset_ns);
#endif
    }
    /*
     */

    prev_restart_guard = current_restart_guard;
    prev_restart_interval = current_restart_interval;

    uint64_t tmp_end = rde_us_to_rtick_ext(time_to_wait);

    if (rtick_passed_ext >= (((uint64_t)tmp_end) << 8)) {
      FATAL("Not enough time! (time passed is bigger than restart time)");
    } else if (guards_rtick_ext >= (((uint64_t)tmp_end) << 8)) {
      FATAL("Not enough time! (requested guards are bigger than restart time)");
    } else if (offset_ns >=
               SAMU_PRE_EPOCH_PROCEDURE_GUARD_US * 1000 / DWT_TICK_TO_NS_32 +
                   STATETIME_PRE_EPOCH_DURATION + PRE_EPOCH_DURATION) {
      FATAL("offset_ns is bigger than the uwb guards");
    } else {
      guards_rtick_ext = utr_uwb_to_rtick_ext(
          SAMU_PRE_EPOCH_PROCEDURE_GUARD_US * 1000 / DWT_TICK_TO_NS_32 +
          STATETIME_PRE_EPOCH_DURATION + PRE_EPOCH_DURATION - offset_ns);

      if (guards_rtick_ext + guards_rtick_ext >= (((uint64_t)tmp_end) << 8)) {
        FATAL(
            "Not enough time! (requested guards are bigger than restart time)");
      } else if (guards_rtick_ext + guards_rtick_ext + 500 >=
                 (((uint64_t)tmp_end) << 8)) {
        FATAL("Not enough time! (due to watchdog guards)");
      } else {
        // Calculate time at which the platform should be brought back up
        next_epoch_start = (((uint64_t)start) << 15) +
                           (((uint64_t)tmp_end) << 8) - rtick_passed_ext;
        start_end = (next_epoch_start - guards_rtick_ext) >> 15;

        watchdog_end = start_end - 500;

        if (RTIMER_CLOCK_DIFF(watchdog_end, RTIMER_NOW()) <= 0) {
          FATAL("No remaining time to set the restart of the epoch (stage1)");
        }

        rtimer_set(&rt, watchdog_end, 1, contiki_samu_wait_for_epoch_start,
                   NULL);
      }
    }
  }

  PROCESS_END();
}
