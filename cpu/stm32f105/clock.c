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
 *		Contiki Clock architecture dependent driver
 *
 * \author
 * 		Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "stm32f10x.h"
#include "sys/clock.h"
#include "sys/etimer.h"
#include "sys/energest.h"
/*---------------------------------------------------------------------------*/
static volatile clock_time_t current_clock = 0;
static volatile unsigned long current_seconds = 0;
static unsigned int second_countdown = CLOCK_SECOND;
/*---------------------------------------------------------------------------*/
/* Tick timer interrupt handler to replace the default one. */
void
SysTick_Handler(void)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  /* Increase tick current_clock */
  current_clock++;
  
  /* Inform the etimer library about a possible etimer expiration */
  if(etimer_pending()) {
    etimer_request_poll();
  }

  /* Increase second count */
  if(--second_countdown == 0) {
    current_seconds++;
    second_countdown = CLOCK_SECOND;
  }

  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
/**
 * Initialize the clock library.
 *
 * This function initializes the clock library and should be called
 * from the main() function of the system.
 *
 */
void
clock_init(void)
{
	if(SysTick_Config(SystemCoreClock / CLOCK_SECOND)) {
		/* Capture error */
		while (1);
	}
	NVIC_SetPriority(SysTick_IRQn, 5);
}
/*---------------------------------------------------------------------------*/
/**
 * Get the current clock time.
 *
 * This function returns the current system clock time.
 *
 * \return The current clock time, measured in system ticks.
 */
CCIF clock_time_t
clock_time(void)
{
  return current_clock;
}
/*---------------------------------------------------------------------------*/
/**
 * Get the current value of the platform seconds.
 *
 * This could be the number of seconds since startup, or
 * since a standard epoch.
 *
 * \return The value.
 */
CCIF unsigned long
clock_seconds(void)
{
  return current_seconds;
}
/*---------------------------------------------------------------------------*/
/**
 * Set the value of the platform seconds.
 * \param sec   The value to set.
 *
 */
void
clock_set_seconds(unsigned long sec)
{
  current_seconds = sec;
}
/*---------------------------------------------------------------------------*/
/**
 * Wait for a given number of ticks.
 * \param t   How many ticks.
 *
 */
void
clock_wait(clock_time_t t)
{
  clock_time_t start;

  start = clock_time();
  while(clock_time() - start < (clock_time_t) t);
}
/*---------------------------------------------------------------------------*/
/**
 * Delay a given number of microseconds.
 * \param dt   How many microseconds to delay.
 *
 * \note Interrupts could increase the delay by a variable amount.
 */
void
clock_delay_usec(uint16_t dt)
{
  /* Function obtained from usb_bsp.c file */
  uint32_t count = 0;
  const uint32_t utime = (120 * dt / 7);

  do {
    if(++count > utime) {
      return ;
    }
  } while (1);
}
/*---------------------------------------------------------------------------*/