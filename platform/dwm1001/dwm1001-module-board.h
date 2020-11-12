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

#ifndef DWM1001_H
#define DWM1001_H

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

#endif /* DWM1001_H */
