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
 *    STM32F105 specific rtimer code
 *
 * \author
 *    Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "contiki.h"
#include "sys/rtimer.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"
#include "leds.h"
/*---------------------------------------------------------------------------*/
void
RTCAlarm_IRQHandler(void)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  if(RTC_GetITStatus(RTC_FLAG_ALR) != RESET) {
    /* Clear EXTI line17 pending bit */
    EXTI_ClearITPendingBit(EXTI_Line17);

    /* Check if the Wake-Up flag is set */
    if(PWR_GetFlagStatus(PWR_FLAG_WU) != RESET) {
      /* Clear Wake Up flag */
      PWR_ClearFlag(PWR_FLAG_WU);
    }

    /* Wait until last write operation on RTC registers has finished */
    RTC_WaitForLastTask();
    /* Clear the RTC Alarm interrupt */
    RTC_ClearITPendingBit(RTC_IT_ALR);
  }

  /* Call the rtimer callback function and schedule the next rtimer task */
  rtimer_run_next();

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_init()
{
  EXTI_InitTypeDef exti_conf;
  NVIC_InitTypeDef nvic_conf;

  /* Enable EXTI Line 17 (RTC Alarm) to generate an interrupt on rising edge */
  EXTI_ClearITPendingBit(EXTI_Line17);
  exti_conf.EXTI_Line = EXTI_Line17;
  exti_conf.EXTI_Mode = EXTI_Mode_Interrupt;
  exti_conf.EXTI_Trigger = EXTI_Trigger_Rising;
  exti_conf.EXTI_LineCmd = ENABLE;
  EXTI_Init(&exti_conf);

  /* Configure the NVIC RTC Alarm Interrupt */
  nvic_conf.NVIC_IRQChannel = RTCAlarm_IRQn;
  nvic_conf.NVIC_IRQChannelPreemptionPriority = 3;
  nvic_conf.NVIC_IRQChannelSubPriority = 0;
  nvic_conf.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&nvic_conf);
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_schedule(rtimer_clock_t t)
{
  RTC_SetAlarm(t);
}
/*---------------------------------------------------------------------------*/
rtimer_clock_t
rtimer_arch_now(void)
{
  return RTC_GetCounter();
}
/*---------------------------------------------------------------------------*/
