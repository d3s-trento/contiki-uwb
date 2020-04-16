/* Copyright (c) 2014 Nordic Semiconductor. All Rights Reserved.
 * Copyright (c) 2018, University of Trento, Italy
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include "nrfx_config.h"
#include "serial_baudrate.h"

#ifndef DWM1001_DEV_H
#define DWM1001_DEV_H

/*---------------------------------------------------------------------------*/
/* LEDs definitions for DWM1001_DEV_H */
#define LEDS_CONF_ALL    15 /* Only 4 LEDs are for the user */
#define LEDS_1          1 /* Green */
#define LEDS_2          2 /* Red */
#define LEDS_3          4 /* Red */
#define LEDS_4          8 /* Blue */

#define LEDS_GREEN      LEDS_1
#define LEDS_ORANGE     LEDS_2 /* Red */
#define LEDS_RED        LEDS_3
#define LEDS_BLUE     	LEDS_4 /* Blue */

/*---------------------------------------------------------------------------*/
/* LED GPIO Port and Pins */
#define DWM1001_LED_1 30 /* Green */
#define DWM1001_LED_2 14 /* Red */ /* This at the moment didn't work....*/
#define DWM1001_LED_3 22 /* Red */
#define DWM1001_LED_4 31 /* Red */

#define DWM1001_LED_1_MASK 1 << DWM1001_LED_1
#define DWM1001_LED_2_MASK 1 << DWM1001_LED_2
#define DWM1001_LED_3_MASK 1 << DWM1001_LED_3
#define DWM1001_LED_4_MASK 1 << DWM1001_LED_4

#define DWM1001_LEDS_MASK (DWM1001_LED_1_MASK | DWM1001_LED_2_MASK | DWM1001_LED_3_MASK | DWM1001_LED_4_MASK)
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* DW1000 Architecture-dependent pins */
#define DW1000_RST    24
#define DW1000_IRQ_EXTI 19
#define SPI_CS_PIN   17
#define SPI_INSTANCE  1 /* SPI instance index */

/*---------------------------------------------------------------------------*/

/* SPIM1 connected to DW1000 */
#define SPI1_SCK_PIN   16  /* DWM1001 SPIM1 sck connected to DW1000 */
#define SPI1_MOSI_PIN  20  /* DWM1001 SPIM1 mosi connected to DW1000 */
#define SPI1_MISO_PIN  18  /* DWM1001 SPIM1 miso connected to DW1000 */
#define SPI1_IRQ_PRIORITY 6 // APP_IRQ_PRIORITY_LOW /* */
// #define SPI1_SS_PIN    XX  /*  Not used with DMW1001 */

/* Low frequency clock source to be used by the SoftDevice */
#define NRF_CLOCK_LFCLKSRC      { .source = NRF_CLOCK_LF_SRC_XTAL, \
                                  .rc_ctiv = 0, \
                                  .rc_temp_ctiv = 0, \
                                  .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM }

/*---------------------------------------------------------------------------*/
/* UART configuration, comunication parater are in nrfx_config and sdk_config */
#define UART0_TX_PIN_NUMBER 5
#define UART0_RX_PIN_NUMBER 11

#define UART0_ENABLED NRFX_UART0_ENABLED

#ifdef UART_CONF_BAUDRATE
#define UART_DEFAULT_BAUDRATE UART_CONF_BAUDRATE
#else
#define UART_DEFAULT_BAUDRATE NRF5_SERIAL_BAUDRATE_115200 /*defined in serial_baudarte.h*/
#endif

#endif /* DWM1001_DEV_H */
