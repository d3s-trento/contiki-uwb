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
 *    CIR Printing Functions
 *
 * \author
 *    Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "contiki.h"
#include "deca_regs.h"
/*---------------------------------------------------------------------------*/
/* Number of samples in the accumulator register depending on PRF */
/* Each sample is formed by a 16-bit real + 16-bit imaginary number */
#define ACC_LEN_PRF16 992
#define ACC_LEN_PRF64 1016
/*---------------------------------------------------------------------------*/
/* TO DO: Adapt ACC_LEN_BYTES at runtime depending on PRF configuration */
#ifdef ACC_LEN_BYTES_CONF
#define ACC_LEN_BYTES ACC_LEN_BYTES_CONF
#else
#define ACC_LEN_BYTES (ACC_LEN_PRF64 * 4)
#endif
/*---------------------------------------------------------------------------*/
#define ACC_READ_STEP (128)
/*---------------------------------------------------------------------------*/
void print_cir(void);
void print_cir_samples(uint16_t s1, uint16_t len);
void print_readable_cir(void);
/*---------------------------------------------------------------------------*/
