/**
 * \file
 *      Platform Dependent DW1000 Driver Header File
 *
 */

#ifndef DW1000_ARCH_H_
#define DW1000_ARCH_H_
/*---------------------------------------------------------------------------*/
#include "contiki.h"
#include "stm32f10x.h"
#include "board.h"
/*---------------------------------------------------------------------------*/
#define DW1000_SPI_OPEN_ERROR  0
#define DW1000_SPI_OPEN_OK     1
/*---------------------------------------------------------------------------*/
#define DW1000_SPI_SLOW        SPI_BaudRatePrescaler_32 /* 2.25 MHz */
#define DW1000_SPI_FAST        SPI_BaudRatePrescaler_4 /* 18 MHz */
/*---------------------------------------------------------------------------*/
/* DW1000 IRQ (EXTI9_5_IRQ) handler type. */
typedef void (*dw1000_isr_t)(void);
/* DW1000 IRQ handler declaration. */
/* Function to set a new DW1000 EXTI ISR handler */ 
void dw1000_set_isr(dw1000_isr_t new_dw1000_isr);
/*---------------------------------------------------------------------------*/
void dw1000_arch_init();
void dw1000_arch_reset();
void dw1000_arch_wakeup_nowait();
void dw1000_spi_open(void);
void dw1000_spi_close(void);
void dw1000_spi_read(uint16_t hdrlen, const uint8_t *hdrbuf, uint32_t len, uint8_t *buf);
void dw1000_spi_write(uint16_t hdrlen, const uint8_t *hdrbuf, uint32_t len, const uint8_t *buf);
void dw1000_set_spi_bit_rate(uint16_t brate);
int8_t dw1000_disable_interrupt(void);
void dw1000_enable_interrupt(int8_t irqn_status);
/*---------------------------------------------------------------------------*/
/* Platform-specific bindings for the DW1000 driver */
#define writetospi(cnt, header, length, buffer) dw1000_spi_write(cnt, header, length, buffer)
#define readfromspi(cnt, header, length, buffer) dw1000_spi_read(cnt, header, length, buffer)
#define decamutexon() dw1000_disable_interrupt()
#define decamutexoff(stat) dw1000_enable_interrupt(stat)
#define deca_sleep(t) clock_wait(t) // XXX assumes 1ms tick !!!
/*---------------------------------------------------------------------------*/
#endif /* DW1000_ARCH_H_ */
