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

#include "deca_regs.h"
#include "deca_device_api.h"
#include <math.h>
#include <stdio.h>
#include "dw1000-diag.h"

/* Reads raw diagnostic data from the radio and computes the received signal
 * power levels for the first path and for the overall transmission according
 * to the DW1000 User Manual (4.7.1 "Estimating the signal power in the first
 * path" and 4.7.2 "Estimating the receive signal power").
 * Requires the current configuration to compute PRF and SFD corrections.
 * Results are stored in the dw1000_diagnostics_t structure.
 *
 * False is returned if power levels could not be computed.
 */
bool
dw1000_diagnostics(dw1000_diagnostics_t *d, const dwt_config_t* config) {

  /* Read diagnostics data */
  dwt_readdiagnostics(&d->dwt_rxdiag);

  /* Get non-saturated preamble accumulator count */
  d->pac_nonsat = dwt_read16bitoffsetreg(DRX_CONF_ID, 0x2C);

  /* Compute corrected preamble counter (used for CIR power adjustment) */
  if(d->dwt_rxdiag.rxPreamCount == d->pac_nonsat) {
    uint16_t sfd_correction = (config->dataRate == DWT_BR_110K) ? 64 : 8;
    d->pac_correction = pow(d->dwt_rxdiag.rxPreamCount - sfd_correction, 2);
  }
  else {
    d->pac_correction = pow(d->dwt_rxdiag.rxPreamCount, 2);
  }

  /* Compute the CIR power level, corrected by PAC value */
  d->cir_pwr = d->dwt_rxdiag.maxGrowthCIR * pow(2, 17) / d->pac_correction;

  /* Compute RX power and First-Path RX power */
  d->fp_raw = (pow(d->dwt_rxdiag.firstPathAmp1, 2) + pow(d->dwt_rxdiag.firstPathAmp2, 2) + pow(d->dwt_rxdiag.firstPathAmp3, 2)) / d->pac_correction;
  if(d->cir_pwr != 0 && d->fp_raw != 0) {
    float prf_correction = (config->prf == DWT_PRF_64M) ? 121.74 : 113.77;
    d->rx_pwr = 10 * log10(d->cir_pwr) - prf_correction;
    d->fp_pwr = 10 * log10(d->fp_raw) - prf_correction;
  }
  else {
    d->rx_pwr = 0;
    d->fp_pwr = 0;
    return false;
  }

  return true;
}

/* Print diagnostics results. */
void
dw1000_print_diagnostics(dw1000_diagnostics_t *d) {
  printf("DW1000 Diagnostics pwr:%.2f fp_pwr:%.2f fp:%u(%u,%u,%u) cir_pwr(raw):%f(%u) pac(nonsat):%u(%u) max_noise:%u std_noise:%u\n",
    d->rx_pwr,
    d->fp_pwr,
    d->dwt_rxdiag.firstPath,
    d->dwt_rxdiag.firstPathAmp1,
    d->dwt_rxdiag.firstPathAmp2,
    d->dwt_rxdiag.firstPathAmp3,
    d->cir_pwr,
    d->dwt_rxdiag.maxGrowthCIR,
    d->dwt_rxdiag.rxPreamCount,
    d->pac_nonsat,
    d->dwt_rxdiag.maxNoise,
    d->dwt_rxdiag.stdNoise
  );
}
