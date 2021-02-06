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
 *      Platform Dependent DW1000 Driver Source File
 *
 */

#include "contiki.h"
/*---------------------------------------------------------------------------*/
#include "nrf.h"
#include "nrfx_spim.h"
#include "nrfx_gpiote.h"
#include "nrf_delay.h"
#include "app_util_platform.h"
#include "app_error.h"
/*---------------------------------------------------------------------------*/
#include "sys/clock.h"
/*---------------------------------------------------------------------------*/
#include "leds.h"
/*---------------------------------------------------------------------------*/
#include "deca_device_api.h"
#include "deca_types.h"
#include "deca_regs.h"
/*---------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#include "dw1000-arch.h"
#include "dw1000.h"


#define DEBUG_LEDS 0
#undef LEDS_TOGGLE
#if DEBUG_LEDS
#define LEDS_TOGGLE(x) leds_toggle(x)
#else
#define LEDS_TOGGLE(x)
#endif
/*---------------------------------------------------------------------------*/
#define NRFX_SPIM_DEFAULT_CONFIG_2M			     \
  {                                                          \
   .sck_pin      = DW1000_SPI_CLK_PIN,			     \
   .mosi_pin     = DW1000_SPI_MOSI_PIN,			     \
   .miso_pin     = DW1000_SPI_MISO_PIN,			     \
   .ss_pin       = NRFX_SPIM_PIN_NOT_USED,		     \
   .ss_active_high = false,			             \
   .irq_priority = (APP_IRQ_PRIORITY_MID - 2),		     \
   .orc          = 0xFF,				     \
   .frequency    = NRF_SPIM_FREQ_2M,			     \
   .mode         = NRF_SPIM_MODE_0,			     \
   .bit_order    = NRF_SPIM_BIT_ORDER_MSB_FIRST,	     \
  }

#define NRFX_SPIM_DEFAULT_CONFIG_8M			     \
  {                                                          \
   .sck_pin      = DW1000_SPI_CLK_PIN,			     \
   .mosi_pin     = DW1000_SPI_MOSI_PIN,			     \
   .miso_pin     = DW1000_SPI_MISO_PIN,			     \
   .ss_pin       = NRFX_SPIM_PIN_NOT_USED,		     \
   .ss_active_high = false,		    	             \
   .irq_priority = (APP_IRQ_PRIORITY_MID - 2),		     \
   .orc          = 0xFF,				     \
   .frequency    = NRF_SPIM_FREQ_8M,			     \
   .mode         = NRF_SPIM_MODE_0,			     \
   .bit_order    = NRF_SPIM_BIT_ORDER_MSB_FIRST,	     \
  }
/*---------------------------------------------------------------------------*/
static const nrfx_spim_t spim = NRFX_SPIM_INSTANCE(DW1000_SPIM_INSTANCE);
/*---------------------------------------------------------------------------*/
/* Forward declarations */
void dw1000_spi_set_slow_rate(void);
void dw1000_spi_set_fast_rate(void);
/*---------------------------------------------------------------------------*/
static dw1000_isr_t dw1000_isr = NULL; // no ISR by default
static volatile int dw1000_irqn_status;
/*---------------------------------------------------------------------------*/
/* DW1000 interrupt handler */
static void
dw1000_irq_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
  ENERGEST_ON(ENERGEST_TYPE_IRQ);
  dw1000_irqn_status = 0; // we are in the interrupt handler, we set the
                          // interrupt status to 'disabled' so that the
                          // SPI functions do not disable/enable it in vain
                          // (which is slow on this platform as it requires
                          // a function call)
  do {
    if(dw1000_isr != NULL) {
      dw1000_isr();
    }
  } while(nrfx_gpiote_in_is_set(DW1000_IRQ_EXTI) == true);
  dw1000_irqn_status = 1; // Marking it 'enabled' again
  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
}
/*---------------------------------------------------------------------------*/
int
dw1000_disable_interrupt(void)
{
  if(dw1000_irqn_status != 0) {
    nrfx_gpiote_in_event_disable(DW1000_IRQ_EXTI);
    //nrfx_gpiote_in_event_enable(DW1000_IRQ_EXTI, false);
    dw1000_irqn_status = 0;
    return 1; // previous status was 'enabled'
  }
  else {
    return 0; // previous status was 'disabled'
  }
}
/*---------------------------------------------------------------------------*/
void
dw1000_enable_interrupt(int previous_irqn_status)
{
  if(previous_irqn_status != 0) {
    nrfx_gpiote_in_event_enable(DW1000_IRQ_EXTI, true);
    dw1000_irqn_status = 1;
  }
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_set_slow_rate(void)
{
  nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG_2M;
  nrfx_spim_uninit(&spim);
  APP_ERROR_CHECK(nrfx_spim_init(&spim, &spi_config, NULL, NULL));
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_set_fast_rate(void)
{
  nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG_8M;
  nrfx_spim_uninit(&spim);
  APP_ERROR_CHECK(nrfx_spim_init(&spim, &spi_config, NULL, NULL));
}
/*---------------------------------------------------------------------------*/
void
dw1000_set_isr(dw1000_isr_t new_dw1000_isr)
{
  dw1000_isr = new_dw1000_isr;
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_open(void)
{
}
/*---------------------------------------------------------------------------*/
void
dw1000_spi_close(void)
{
}
/*---------------------------------------------------------------------------*/
int
dw1000_spi_read(uint16_t  headerLength,
                const uint8_t   *headerBuffer,
                uint32_t  readLength,
                uint8_t   *readBuffer)
{
  nrfx_spim_xfer_desc_t const xfer_hdr_desc =
    {
      .p_tx_buffer = headerBuffer,
      .tx_length = headerLength,
      .p_rx_buffer = NULL,
      .rx_length = 0,
    };
  nrfx_spim_xfer_desc_t const xfer_data_desc =
    {
      .p_tx_buffer = NULL,
      .tx_length = 0,
      .p_rx_buffer = readBuffer,
      .rx_length = readLength,
    };

  nrf_gpio_pin_clear(DW1000_SPI_CS_PIN);
  APP_ERROR_CHECK(nrfx_spim_xfer(&spim, &xfer_hdr_desc, 0));
  APP_ERROR_CHECK(nrfx_spim_xfer(&spim, &xfer_data_desc, 0));
  nrf_gpio_pin_set(DW1000_SPI_CS_PIN);

  return 0;
}
/*---------------------------------------------------------------------------*/
int
dw1000_spi_write(uint16_t       headerLength,
                 const uint8_t  *headerBuffer,
                 uint32_t       bodyLength,
                 const uint8_t  *bodyBuffer)
{
  nrfx_spim_xfer_desc_t const xfer_hdr_desc =
    {
      .p_tx_buffer = headerBuffer,
      .tx_length = headerLength,
      .p_rx_buffer = NULL,
      .rx_length = 0,
    };
  nrfx_spim_xfer_desc_t const xfer_data_desc =
    {
      .p_tx_buffer = bodyBuffer,
      .tx_length = bodyLength,
      .p_rx_buffer = NULL,
      .rx_length = 0,
    };

  nrf_gpio_pin_clear(DW1000_SPI_CS_PIN);
  APP_ERROR_CHECK(nrfx_spim_xfer(&spim, &xfer_hdr_desc, 0));
  APP_ERROR_CHECK(nrfx_spim_xfer(&spim, &xfer_data_desc, 0));
  nrf_gpio_pin_set(DW1000_SPI_CS_PIN);

  return 0;
}
/*---------------------------------------------------------------------------*/
void
dw1000_arch_init()
{
  /* configure CS pin as output high */
  nrfx_gpiote_out_config_t cs_config = NRFX_GPIOTE_CONFIG_OUT_SIMPLE(true);
  nrfx_gpiote_out_init(DW1000_SPI_CS_PIN, &cs_config);

  dw1000_arch_reset(); /* Target specific drive of RSTn line into DW1000 low for a period.*/

  /* For initialisation, DW1000 clocks must be temporarily set to crystal speed.
   * After initialisation SPI rate can be increased for optimum performance.
   */
  nrfx_spim_config_t spi_config = NRFX_SPIM_DEFAULT_CONFIG_2M;
  APP_ERROR_CHECK(nrfx_spim_init(&spim, &spi_config, NULL, NULL));

  if (dwt_readdevid() != DWT_DEVICE_ID) {
    printf("Radio sleeping?\n");
    dw1000_arch_wakeup_nowait();
    nrf_delay_ms(5);
    dw1000_arch_reset();
  }

  if(dwt_initialise(DWT_LOADUCODE | DWT_READ_OTP_PID | DWT_READ_OTP_LID |
                    DWT_READ_OTP_BAT | DWT_READ_OTP_TMP)
          == DWT_ERROR) {
    printf("DW1000 INIT FAILED\n");
    while(1) {
      /* If the init function fails, we stop here */
      nrf_delay_ms(500);
      // XXX handle this in a better way!
    }
  }
  dw1000_spi_set_fast_rate();

  /* Enable DW1000 IRQ Pin for external interrupt.
   * NOTE: The DW1000 IRQ Pin should be Pull Down to
   * prevent unnecessary EXT Interrupt while DW1000
   * goes to sleep mode */
  nrfx_gpiote_in_config_t in_config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
  in_config.pull = NRF_GPIO_PIN_PULLDOWN;
  nrfx_gpiote_in_init(DW1000_IRQ_EXTI, &in_config, dw1000_irq_handler);
  nrfx_gpiote_in_event_enable(DW1000_IRQ_EXTI, true);
  dw1000_irqn_status = 1;
}
/*---------------------------------------------------------------------------*/
void
dw1000_arch_reset()
{
  /* Set RST Pin as an output */
  nrf_gpio_cfg_output(DW1000_RST_PIN);

  /* Clear the RST pin to reset the DW1000 */
  nrf_gpio_pin_clear(DW1000_RST_PIN);

  nrf_delay_us(1);

  /* Set the RST pin back as an input */
  nrf_gpio_cfg_input(DW1000_RST_PIN, NRF_GPIO_PIN_NOPULL);

  /* Sleep 2 ms to get the DW1000 restarted */
  nrf_delay_ms(2);
}
/*---------------------------------------------------------------------------*/
/* Note that after calling this function you need to wait 5ms for XTAL to
 * start and stabilise (or wait for PLL lock IRQ status bit: in SLOW SPI mode)
 */
void dw1000_arch_wakeup_nowait() {
  /* To wake up the DW1000 we keep the SPI CS line low for (at least) 500us. */
  nrf_gpio_pin_clear(DW1000_SPI_CS_PIN);
  nrf_delay_us(500);
  nrf_gpio_pin_set(DW1000_SPI_CS_PIN);
}
