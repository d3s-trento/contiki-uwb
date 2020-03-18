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
 *      Contiki DW1000 Advanced Device Diagnostics
 *
 * \author
 *      Davide Vecchia <davide.vecchia@unitn.it>
 */

#ifndef DW1000_DIAG_H_
#define DW1000_DIAG_H_
#include "deca_device_api.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  dwt_rxdiag_t dwt_rxdiag; /* Basic DW1000 diagnostics */
  uint16_t pac_nonsat; /* Non-saturated Preamble Accumulator Count (PAC) */
  uint32_t pac_correction; /* PAC normalization factor, affected by non-saturated PAC */
  float cir_pwr; /* Adjusted CIR power from ((C * 2^17) / N^2)) where C is the raw CIR power and N is the PAC normalization factor */
  float fp_raw; /* First Path Power Level before SFD correction */
  float fp_pwr; /* First Path Power Level */
  float rx_pwr; /* RX Power Level */
} dw1000_diagnostics_t;

/* Reads raw diagnostic data from the radio and computes the received signal
 * power levels for the first path and for the overall transmission according
 * to the DW1000 User Manual (4.7.1 "Estimating the signal power in the first
 * path" and 4.7.2 "Estimating the receive signal power").
 * Requires the current configuration to compute PRF and SFD corrections.
 * Results are stored in the dw1000_diagnostics_t structure.
 *
 * False is returned if power levels could not be computed.
 */
bool dw1000_diagnostics(dw1000_diagnostics_t *d, const dwt_config_t* config);

/* Print diagnostics results */
void dw1000_print_diagnostics(dw1000_diagnostics_t *d);

#endif //DW1000_DIAG_H_
