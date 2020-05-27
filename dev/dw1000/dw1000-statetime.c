#include "dw1000-statetime.h"
#include "dw1000-util.h"
#include "dw1000-config.h"
#include <inttypes.h>
#include <stdbool.h>
/*---------------------------------------------------------------------------*/
#define DEBUG           1
#if DEBUG
#include <stdio.h>
#define PRINTF(...)     printf(__VA_ARGS__)
#else
#define PRINTF(...)     do {} while(0)
#endif

#define ENERGY_LOG(...) PRINTF("[ENERGY]" __VA_ARGS__)
/*---------------------------------------------------------------------------*/
/*                      UTILITY FUNCTIONS DECLARATIONS                       */
/*---------------------------------------------------------------------------*/
static uint32_t estimate_preamble_time_ns();
static uint32_t estimate_payload_time_ns(const uint16_t framelength);
/*---------------------------------------------------------------------------*/
/*                          STATIC VARIABLES                                 */
/*---------------------------------------------------------------------------*/
static dw1000_statetime_context_t context;
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_context_init()
{
    context.state = DW1000_IDLE;

    context.idle_time_us = 0;
    context.rx_preamble_hunting_time_us = 0;
    context.rx_preamble_time_us = 0;
    context.rx_data_time_us = 0;
    context.tx_preamble_time_us = 0;
    context.tx_data_time_us = 0;

    context.tracing = false;
    context.schedule_32hi = 0;

    context.last_idle_32hi = 0;

    context.is_rx_after_tx = 0;
    context.rx_delay_32hi = 0;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_set_last_idle(const uint32_t ts_idle_32hi)
{
    if (!context.tracing) return;

    context.last_idle_32hi = ts_idle_32hi;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_tx(const uint32_t schedule_tx_32hi)
{
    if (!context.tracing) return;

    context.is_rx_after_tx = false;
    context.schedule_32hi  = schedule_tx_32hi;
    context.state = DW1000_SCHEDULED_TX;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_txrx(const uint32_t schedule_tx_32hi, const uint32_t rx_delay_uus)
{
    if (!context.tracing) return;

    context.is_rx_after_tx = true;
    context.schedule_32hi  = schedule_tx_32hi;
    context.rx_delay_32hi  = rx_delay_uus * 1000 / 4;
    context.state = DW1000_SCHEDULED_TX;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_schedule_rx(const uint32_t schedule_rx_32hi)
{
    if (!context.tracing) return;

    context.schedule_32hi = schedule_rx_32hi;
    context.state = DW1000_SCHEDULED_RX;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_tx(const uint32_t sfd_tx_32hi, const uint16_t framelength)
{
    if (!context.tracing) return;

    uint32_t preamble_time_ns = estimate_preamble_time_ns();
    uint32_t payload_time_ns  = estimate_payload_time_ns(framelength);

    context.idle_time_us += ((sfd_tx_32hi - context.last_idle_32hi) * 4 -
            preamble_time_ns) / 1000;
    context.tx_preamble_time_us += preamble_time_ns / 1000;
    context.tx_data_time_us += payload_time_ns / 1000;

    // the radio goes idle when tx done is issued, therefore
    // roughly at sfd + the time required to read the PHY payload
    context.last_idle_32hi = sfd_tx_32hi + payload_time_ns / 4;

    if (context.is_rx_after_tx) {

        // setrxaftertx_delay function used
        context.state = DW1000_RX_AFTER_TX;

    } else {

        context.state = DW1000_IDLE;

    }
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_after_rx(const uint32_t sfd_rx_32hi, const uint16_t framelength)
{
    if (!context.tracing) return;

    uint32_t preamble_time_ns = estimate_preamble_time_ns();
    uint32_t payload_time_ns  = estimate_payload_time_ns(framelength);

    if (context.state == DW1000_SCHEDULED_RX) {

        // using the rx_enable function
        // context.schedule_32hi stores the timestamp the
        // radio switched to rx
        context.idle_time_us += (context.schedule_32hi - context.last_idle_32hi) * 4 / 1000;
        context.rx_preamble_hunting_time_us +=
            ((sfd_rx_32hi - context.schedule_32hi) * 4 - preamble_time_ns) / 1000;

    } else if (context.state == DW1000_RX_AFTER_TX) {

        // rx using the setrxaftertx function
        context.idle_time_us += context.rx_delay_32hi * 4 / 1000;

        context.rx_preamble_hunting_time_us +=
            (
             ((sfd_rx_32hi - context.last_idle_32hi) - context.rx_delay_32hi) * 4 -
             preamble_time_ns
            ) / 1000;

    }
    context.rx_preamble_time_us += preamble_time_ns / 1000;
    context.rx_data_time_us += payload_time_ns / 1000;

    // radio switched to idle when rx_ok callback was issued
    context.last_idle_32hi = sfd_rx_32hi + payload_time_ns / 4;

    context.state = DW1000_IDLE;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_start()
{
    context.tracing = true;
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_stop()
{
    context.state = DW1000_IDLE;
    context.tracing = false;
}
/*---------------------------------------------------------------------------*/
dw1000_statetime_context_t*
dw1000_statetime_get_context()
{
    return &context;
}
/*---------------------------------------------------------------------------*/
/*                              UTILITY FUNCTIONS                            */
/*---------------------------------------------------------------------------*/
static
uint32_t estimate_preamble_time_ns()
{
    return dw1000_estimate_tx_time(dw1000_get_current_cfg(), 0, true);
}
/*---------------------------------------------------------------------------*/
static
uint32_t estimate_payload_time_ns(const uint16_t framelength)
{
    return dw1000_estimate_tx_time(dw1000_get_current_cfg(), framelength, false) -
        dw1000_estimate_tx_time(dw1000_get_current_cfg(), 0, true);
}
/*---------------------------------------------------------------------------*/
void
dw1000_statetime_print()
{
    PRINTF("I %"PRIu64" TP %"PRIu64" TD %"PRIu64" RH %"PRIu64" RP %"PRIu64" RD %"PRIu64"\n",
            context.idle_time_us, context.tx_preamble_time_us, context.tx_data_time_us,
            context.rx_preamble_hunting_time_us, context.rx_preamble_time_us, context.rx_data_time_us);
}

