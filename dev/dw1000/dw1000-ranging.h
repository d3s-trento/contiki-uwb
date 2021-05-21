/*
 * Copyright (c) 2017, University of Trento.
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

/**
 * \file
 *      Contiki DW1000 Driver ranging module
 *
 * \author
 *      Timofei Istomin <tim.ist@gmail.com>
 */

#include "dw1000.h"
#include "dw1000-arch.h"
#include "dw1000-cir.h"
#include "core/net/linkaddr.h"
#include "contiki-conf.h"

#ifndef DW1000_RANGING_H
#define DW1000_RANGING_H

/*---------------------------------------------------------------------------*/

#ifdef DW1000_CONF_COMPENSATE_BIAS
#define DW1000_COMPENSATE_BIAS DW1000_CONF_COMPENSATE_BIAS
#else
#define DW1000_COMPENSATE_BIAS 1
#endif

#ifdef DW1000_CONF_EXTREME_RNG_TIMING
#define DW1000_EXTREME_RNG_TIMING DW1000_CONF_EXTREME_RNG_TIMING
#else
#define DW1000_EXTREME_RNG_TIMING 0
#endif


/* A flag indicating that the CIR index is provided as relative w.r.t. 
 * the first path index.*/
#define DW1000_CIR_IDX_RELATIVE 0

/* A flag indicating that an absolute CIR index is provided. */
#define DW1000_CIR_IDX_ABSOLUTE 1

/* Call this function before every ranging request to enable acquiring RX diagnostics
 * and/or CIR of the last received message.
 *
 * Params:
 *  - cir_idx_mode  DW1000_CIR_IDX_RELATIVE or DW1000_CIR_IDX_ABSOLUTE
 *  - cir_s1        CIR sample index to start reading the CIR from. Might be
 *                  absoulte or relative depending on cir_idx_mode
 *  - n_samples     Number of CIR samples to read to the buffer
 *  - samples       A buffer to read CIR samples to. It must be at least
 *                  (n_samples + 1) long. If NULL, CIR will not be read.
 *
 * After the ranging is done with a successful status, the diagnostics will be available
 * in the associated ranging_data_t structure and the CIR will be read in the specified 
 * buffer (if any).
 */
void dw1000_ranging_acquire_diagnostics(uint16_t cir_idx_mode, int16_t cir_s1, uint16_t n_samples, dw1000_cir_sample_t* samples);

extern process_event_t ranging_event;

typedef struct {
  int status;       /* 1=SUCCESS, 0=FAIL */
  uint16_t cir_samples_acquired;
  double distance;
  double raw_distance;
  double freq_offset;
  dwt_rxdiag_t rxdiag;
} ranging_data_t;

/*---------------------------------------------------------------------------*/
/* Private functions for driver-level use only                               */
/*---------------------------------------------------------------------------*/

/* (Re)initialise the ranging module.
 *
 * Needs to be called before issuing or serving ranging requests and
 * after changing radio parameters. */
void dw1000_ranging_init();

/* Callback to process ranging good frame events */
void
dw1000_rng_ok_cb(const dwt_cb_data_t *cb_data);

/* Callback to process tx confirmation events */
void
dw1000_rng_tx_conf_cb(const dwt_cb_data_t *cb_data);

bool dw1000_range_with(linkaddr_t *lladdr, dw1000_rng_type_t type);
bool dw1000_is_ranging();
void dw1000_range_reset();

#endif /* DW1000_RANGING_H */
