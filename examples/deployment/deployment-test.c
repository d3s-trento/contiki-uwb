#include <stdio.h>
#include <inttypes.h>

#include "contiki.h"
#include "dev/leds.h"
#include "net/netstack.h"
#include "deployment.h"


PROCESS(deployment_test, "Deployment test");
AUTOSTART_PROCESSES(&deployment_test);

/*------------------------------------------------------------------------------------------------*/
PROCESS_THREAD(deployment_test, ev, data)
{
  static struct etimer et;
  PROCESS_BEGIN();

  deployment_set_node_id_ieee_addr();

  while(1) {
    etimer_set(&et, CLOCK_SECOND * 1);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    printf("----------------------------------------------------------------------\n");
    deployment_print_id_info();

    uint8_t ieee_addr[IEEE_ADDR_LEN] = {0};
    NETSTACK_RADIO.get_object(RADIO_PARAM_64BIT_ADDR, ieee_addr, IEEE_ADDR_LEN);

    if (node_id == 0) {
      printf("[DEPLOYMENT] WARNING: node ID not assigned!\n");
    }
    printf("[DEPLOYMENT] C code: {0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x}\n",
         ieee_addr[0], ieee_addr[1],
         ieee_addr[2], ieee_addr[3],
         ieee_addr[4], ieee_addr[5],
         ieee_addr[6], ieee_addr[7]);
  }

  PROCESS_END();
}

