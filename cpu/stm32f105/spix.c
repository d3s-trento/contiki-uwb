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
 *         SPIx Source File
 * \author
 *         Pablo Corbalan <p.corbalanpelegrin@unitn.it>
 */

#include "spix.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "stm32f10x_gpio.h"
/*---------------------------------------------------------------------------*/
void
spix_init(SPI_TypeDef *SPIx, SPI_InitTypeDef *spix_conf)
{
  GPIO_InitTypeDef gpio_conf;

  if(SPIx == SPI1) {

    /* Enable SPI1 clock */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);

    /* SPI1 SCK and MOSI Configuration */
    gpio_conf.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio_conf.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio_conf);

    /* SPI1 MISO Configuration */
    gpio_conf.GPIO_Pin = GPIO_Pin_6;
    gpio_conf.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &gpio_conf);

  } else if (SPIx == SPI2) {

    /* Enable SPI2 clock */
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);

    /* SPI2 SCK and MOSI Configuration */
    gpio_conf.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_15;
    gpio_conf.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio_conf.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio_conf);

    /* SPI2 MISO Configuration */
    gpio_conf.GPIO_Pin = GPIO_Pin_14;
    gpio_conf.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &gpio_conf);

  } else {
    /* We only support SPI1 and SPI2 */
    return;
  }

  /* Init SPIx */
  SPI_Init(SPIx, spix_conf);

  /* Enable SPIx */
  SPI_Cmd(SPIx, ENABLE);
}
/*---------------------------------------------------------------------------*/
void
spix_change_speed(SPI_TypeDef *SPIx, uint16_t speed)
{
  uint16_t reg_status;
  reg_status = 0;

  /* Get the SPIx CR1 value */
  reg_status = SPIx->CR1;

  /* Clear BR[2:0], i.e., bits 3, 4, and 5 */
  reg_status &= 0xFFC7;

  /* Set the new prescaler on BR[2:0] */
  reg_status |= speed;

  /* Write to SPIx CR1 */
  SPIx->CR1 = reg_status;
}
/*---------------------------------------------------------------------------*/
