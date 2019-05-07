/*
 * Copyright (c) 2018, University of Trento, Italy
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``as-is'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
/**
 * \file
 *      Platform Dependent DW1000 Driver Header File
 *
 */

#ifndef DW1000_ARCH_H_
#define DW1000_ARCH_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "deca_types.h"
/*---------------------------------------------------------------------------*/
/* DW1000 IRQ (EXTI9_5_IRQ) handler type. */
typedef void (*dw1000_isr_t)(void);
/* DW1000 IRQ handler declaration. */
extern dw1000_isr_t dw1000_isr;
/* Function to set a new DW1000 EXTI ISR handler */ 
void dw1000_set_isr(dw1000_isr_t new_dw1000_isr);
/*---------------------------------------------------------------------------*/
void dw1000_arch_init();
void dw1000_arch_reset();
void dw1000_spi_open(void);
void dw1000_spi_close(void);
int dw1000_spi_read(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, uint8 *buf);
int dw1000_spi_write(uint16 hdrlen, const uint8 *hdrbuf, uint32 len, const uint8 *buf);
int dw1000_disable_interrupt(void);
void dw1000_enable_interrupt(int irqn_status);
/*---------------------------------------------------------------------------*/
/* Platform-specific bindings for the DW1000 driver */
#define writetospi(cnt, header, length, buffer) dw1000_spi_write(cnt, header, length, buffer)
#define readfromspi(cnt, header, length, buffer) dw1000_spi_read(cnt, header, length, buffer)
#define decamutexon() dw1000_disable_interrupt()
#define decamutexoff(stat) dw1000_enable_interrupt(stat)
#define deca_sleep(t) clock_wait(t)
/*---------------------------------------------------------------------------*/
#endif /* DW1000_ARCH_H_ */
