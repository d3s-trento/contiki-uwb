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
 * \file      Synchronized Action Manager for UWB (SAMU) RAL APIs
 *
 * TSM (code) authors:
 * \author    Timofei Istomin     <tim.ist@gmail.com>
 * \author    Diego Lobba         <diego.lobba@gmail.com>
 *
 * SAMU and Flick (code) authors:
 * \author    Enrico Soprana      <enrico.soprana@unitn.it>
 */

#ifndef SAMU_RAL_H
#define SAMU_RAL_H

#include "rtimer.h"

#include "samu-ral-status.h"
#include <stdbool.h>
#include <stdint.h>

#if SAMU_SUPPORT_CIR
#include "dw1000-cir.h"
#endif

typedef uint32_t radio_status_t; // low-level radio status

typedef struct samu_ral_stats_t {

  // RX error
  uint16_t n_phe;   /* PHR errors */
  uint16_t n_sfdto; /* SFD timeouts */
  uint16_t n_rse;   /* Errors in Reed Solomon decoding phase */
  uint16_t n_fcse;  /* FCS (CRC) errors */
  uint16_t n_rej;   /* Rejections due to frame filtering */

  // RX timeout
  uint16_t n_fto; /* Received Frame Wait Timeout counter */
  uint16_t n_pto; /* Preamble Detection Timeout counter */

  // Others
  uint16_t n_unknown;

  // Success
  uint16_t n_rxok;
  uint16_t n_txok;
} samu_rald_stats_t;

typedef struct {
  uint32_t trx_sfd_time_4ns;   // SFD time of the last TX or RX
  radio_status_t radio_status; // radio status
  enum samu_ral_status status; // slot operation status
  uint8_t *buffer;             // packet buffer
  uint8_t payload_len;         // length of the received or transmitted packet
} samu_rald_slot_t;

#define SAMU_RALD_PLD_OFFS 0

typedef void (*samu_rald_slot_cb)(const samu_rald_slot_t *slot);

struct sniff_conf_t {
  bool enable;
  uint8_t rx_pacs;
  uint8_t sleep_us;
};

struct sniff_conf_t samu_rald_default_sniff_conf();
uint32_t samu_rald_sniff_conf_get_guard_offset(struct sniff_conf_t conf);
bool samu_rald_sniff_conf_eq(struct sniff_conf_t a, struct sniff_conf_t b);
void samu_rald_sniff(struct sniff_conf_t conf);
uint32_t samu_rald_now_time();
uint32_t samu_rald_time_between_us(uint32_t after, uint32_t before);
uint32_t samu_rald_get_pac_duration();
int samu_rald_tx_at(uint8_t *buffer, uint8_t payload_len,
                    uint32_t sfd_time_4ns);
int samu_rald_rx_slot(uint8_t *buffer, uint32_t expected_sfd_time_4ns,
                      uint32_t deadline_4ns);
int samu_rald_rx_until(uint8_t *buffer, uint32_t deadline_4ns);
int samu_rald_rx(uint8_t *buffer);
int samu_rald_rx_from(uint8_t *buffer, uint32_t rx_on_4ns);
int samu_rald_set_timer(uint32_t deadline_4ns);
void samu_rald_set_rx_slot_preambleto(const uint32_t preambleto);
uint32_t samu_rald_get_base_preambleto();
void samu_rald_init();
void samu_rald_set_slot_callback(samu_rald_slot_cb callback);
void samu_rald_stats_get(samu_rald_stats_t *);
void samu_rald_stats_print();
void samu_rald_stats_reset();
void samu_rald_sleep();
bool samu_rald_wakeup();
bool samu_rald_prepare_radio();
bool samu_rald_is_asleep();
bool samu_rald_valid_rmarker(uint32_t radio_status);
int32_t samu_rald_time_passed_from_us(uint32_t start);
uint32_t samu_rald_time_after(uint32_t timestamp);
int32_t samu_rald_time_until_us(uint32_t timestamp);
bool samu_rald_pre_epoch_procedure(uint32_t epoch_start);

struct radio_diagnostic_req {
  bool rxTimestamp;
  bool txTimestamp;
  bool fpIdx;
  bool noise;
  bool firstPathAmplitudes;
  bool maxGrowthCIR;
  bool rxPreamCounts;
  bool peakPath;
#if SAMU_SUPPORT_CIR
  uint16_t start_CIR;
  uint16_t length_CIR;
#endif
};

#ifndef MAX_CIR_SIZE
#define MAX_CIR_SIZE 1016
#endif

struct radio_diagnostic {
  uint64_t rxTimestamp;
  uint64_t txTimestamp;

  // Descriptions taken from the original deca_device_api.h see the manual for
  // more information
  uint16_t fpIdx; // First path index (10.6 bits fixed point integer)

  uint16_t maxNoise; // LDE max value of noise
  uint16_t stdNoise; // Standard deviation of noise

  uint16_t firstPathAmp1; // Amplitude at floor(index FP) + 1
  uint16_t firstPathAmp2; // Amplitude at floor(index FP) + 2
  uint16_t firstPathAmp3; // Amplitude at floor(index FP) + 3

  uint16_t maxGrowthCIR; // Channel Impulse Response max growth CIR

  uint16_t rxPreamCount; // Count of preamble symbols accumulated
  uint16_t pacNonsat;    // Non-saturated Preamble Accumulator Count (PAC)

  uint16_t peakPath;    // Peak path index
  uint16_t peakPathAmp; // Amplitude of the peak

#if SAMU_SUPPORT_CIR
  dw1000_cir_sample_t CIR[MAX_CIR_SIZE + 1];
#endif
};

struct radio_diagnostic_req samu_rald_default_radio_diagnostic_req();
struct radio_diagnostic
samu_rald_get_radio_diagnostic(struct radio_diagnostic_req req);

void samu_rald_tx_fp();
int samu_rald_tx_at_fp(uint32_t sfd_time_4ns);
int samu_rald_rx_slot_fp(uint32_t expected_sfd_time_4ns, uint32_t deadline_4ns);

void fs_debug_log_print();

#endif // SAMU_RAL_H
