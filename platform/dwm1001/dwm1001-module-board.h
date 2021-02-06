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
#define DW1000_RST_PIN        24
#define DW1000_IRQ_EXTI       19

/*---------------------------------------------------------------------------*/
#define DW1000_SPIM_INSTANCE  0 /* SPIM instance index */
#define DW1000_SPI_CS_PIN    17
#define DW1000_SPI_CLK_PIN   16  /* DWM1001 SPIM1 sck connected to DW1000 */
#define DW1000_SPI_MOSI_PIN  20  /* DWM1001 SPIM1 mosi connected to DW1000 */
#define DW1000_SPI_MISO_PIN  18  /* DWM1001 SPIM1 miso connected to DW1000 */

/* Low frequency clock source to be used by the SoftDevice */
#define NRF_CLOCK_LFCLKSRC      { .source = NRF_CLOCK_LF_SRC_XTAL, \
                                  .rc_ctiv = 0, \
                                  .rc_temp_ctiv = 0, \
                                  .xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM }

#endif /* DWM1001_H */
