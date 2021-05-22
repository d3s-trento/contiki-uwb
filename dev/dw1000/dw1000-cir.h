/*
 * Copyright (c) 2018, University of Trento.
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
 * \file
 *    CIR Reading and Printing Functions
 *
 * \author
 *    Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 *    Timofei Istomin <tim.ist@gmail.com>
 */

#ifndef DW1000_CIR_H
#define DW1000_CIR_H

#include <stdbool.h>
#include "contiki.h"
#include "deca_regs.h"

/* Number of 4-byte samples in the accumulator register depending on PRF */
#define DW1000_CIR_LEN_PRF16 992
#define DW1000_CIR_LEN_PRF64 1016
#define DW1000_CIR_MAX_LEN DW1000_CIR_LEN_PRF64

/* Type holding a single CIR sample.
 * Each sample is formed by a 16-bit real + 16-bit imaginary number */
typedef union dw1000_cir_sample
{
  uint32_t u32;
  struct {
    signed real:16;
    signed imag:16;
  } compl;
} dw1000_cir_sample_t;

#define DW1000_CIR_SAMPLE_SIZE (sizeof(dw1000_cir_sample_t))
_Static_assert (DW1000_CIR_SAMPLE_SIZE == 4, "Wrong CIR sample size");

/*---------------------------------------------------------------------------*/
uint16_t dw1000_read_cir(uint16_t s1, uint16_t n_samples, dw1000_cir_sample_t* samples);
uint16_t dw1000_print_cir_from_radio();
uint16_t dw1000_print_cir_samples_from_radio(int16_t s1, uint16_t n_samples);
/*---------------------------------------------------------------------------*/

/* Print CIR buffer in hex.
 *
 * NB! This is a blocking function. If USB output is used, it will block
 * in busy waiting until the whole CIR is printed. If the host is too slow
 * to poll USB or it is not polling, or it is not connected, this
 * function will **BLOCK FOREVER**. Probably, watchdog will reboot the device.
 */
void dw1000_print_cir_hex(dw1000_cir_sample_t* cir, uint16_t n_samples);

#endif // DW1000_CIR_H
