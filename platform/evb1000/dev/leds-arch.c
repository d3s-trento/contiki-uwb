/*
 * Copyright (c) 2017, University of Trento.
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
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
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
 */

/**
 * \file
 *         EVB1000 Platform Dependent LEDs Driver
 * \author
 *         Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "contiki.h"
#include "dev/leds.h"
#include "stm32f10x.h"
#include "board.h"
/*---------------------------------------------------------------------------*/
static unsigned char c;
static int inited = 0;
/*---------------------------------------------------------------------------*/
void
leds_arch_init(void)
{
  if(inited) {
    return;
  }
  inited = 1;

  /* GPIO configuration structure */
  GPIO_InitTypeDef gpio_conf;

  /* Configure LED pins as output and configure their PIN speed */
  gpio_conf.GPIO_Pin = EVB1000_LED_5 | EVB1000_LED_6 | EVB1000_LED_7 | EVB1000_LED_8;
  gpio_conf.GPIO_Mode = GPIO_Mode_Out_PP;
  gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(EVB1000_LEDS_PORT, &gpio_conf);

  /* Is this required? Is it connected to the LEDs? */
  /* GPIO_PinRemapConfig(GPIO_Remap_SPI1, DISABLE); */

  /* Switch off all LEDs */
  GPIO_ResetBits(EVB1000_LEDS_PORT, EVB1000_LED_ALL);
}
/*---------------------------------------------------------------------------*/
unsigned char
leds_arch_get(void)
{
  return c;
}
/*---------------------------------------------------------------------------*/
void
leds_arch_set(unsigned char leds)
{
  c = leds;

  /* Switch off all LEDs */
  GPIO_ResetBits(EVB1000_LEDS_PORT, EVB1000_LED_ALL);

  /* Switch on the corresponding LEDs */
  if((leds & LEDS_YELLOW) == LEDS_YELLOW) {
    GPIO_SetBits(EVB1000_LEDS_PORT, EVB1000_LED_5);
  }
  if((leds & LEDS_RED) == LEDS_RED) {
    GPIO_SetBits(EVB1000_LEDS_PORT, EVB1000_LED_6);
  }
  if((leds & LEDS_GREEN) == LEDS_GREEN) {
    GPIO_SetBits(EVB1000_LEDS_PORT, EVB1000_LED_7);
  }
  if((leds & LEDS_ORANGE) == LEDS_ORANGE) {
    GPIO_SetBits(EVB1000_LEDS_PORT, EVB1000_LED_8);
  }
}
/*---------------------------------------------------------------------------*/
