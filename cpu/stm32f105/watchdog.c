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
/*
 * \file
 *    Independent Watchdog timer
 *
 * \author
 *    Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "dev/watchdog.h"
#include "stm32f10x.h"
#include "stm32f10x_iwdg.h"
/*---------------------------------------------------------------------------*/
static __IO uint32_t lsi_freq = 40000;
/*---------------------------------------------------------------------------*/
void
watchdog_init(void)
{
  /* Enable write access to IWDG_PR and IWDG_RLR registers */
  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);

  /* IWDG counter clock: LSI / 16 */
  IWDG_SetPrescaler(IWDG_Prescaler_16);

  /*
   * Set counter reload value to obtain 1s IWDG Timeout.
   * The maximum timeout with a 16 prescaler divider and LSI = 40 KHz is 
   * 1638.4 ms for a counter reload value of 0xFFF. To have a timeout 
   * of approximately 1s, we set the counter to 0x9C4:
   * 
   * Counter Reload Value = 1000ms * 4095 / 1638.4 ms = 2500 = 0x9C4
   */
  IWDG_SetReload(0x9C4);
}
/*---------------------------------------------------------------------------*/
void
watchdog_start(void)
{
  /* Reload IWDG counter */
  IWDG_ReloadCounter();

  /* Enable IWDG (the LSI oscillator will be enabled by hardware) */
  IWDG_Enable();
}
/*---------------------------------------------------------------------------*/
void
watchdog_periodic(void)
{
  /* Reload IWDG counter */
  IWDG_ReloadCounter();
}
/*---------------------------------------------------------------------------*/
void
watchdog_stop(void)
{
  /* This watchdog cannot stop */
}
/*---------------------------------------------------------------------------*/
void
watchdog_reboot(void)
{
  watchdog_start();
  while(1);
}
/*---------------------------------------------------------------------------*/