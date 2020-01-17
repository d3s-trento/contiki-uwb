#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include "sys/node-id.h"
#include "net/netstack.h"
#include "deployment.h"
#include "node-map.c"

unsigned short int node_id = 0;

/* The total number of nodes in the deployment */
#define N_NODES ((sizeof(deployment_id_addr_list)/sizeof(struct id_addr)))

/* Returns the total number of nodes in the deployment */
uint16_t deployment_get_n_nodes(void)
{
  return N_NODES;
}

uint8_t deployment_set_node_id_ieee_addr(void)
{
  /*
   * Read the 64 bit address stored in the Contiki netstack.
   * This assumes the field to be already set.
   */
  uint8_t ieee_addr[IEEE_ADDR_LEN] = {0};
  NETSTACK_RADIO.get_object(RADIO_PARAM_64BIT_ADDR, ieee_addr, IEEE_ADDR_LEN);

  for (int i=0; i<N_NODES; i++) {
    if (memcmp(ieee_addr, deployment_id_addr_list[i].ieee_addr, IEEE_ADDR_LEN) == 0) {
      node_id = deployment_id_addr_list[i].id;
      return 1;
    }
  }

  return 0;
}

void deployment_print_id_info(void)
{
  uint8_t ieee_addr[IEEE_ADDR_LEN] = {0};
  NETSTACK_RADIO.get_object(RADIO_PARAM_64BIT_ADDR, ieee_addr, IEEE_ADDR_LEN);

  printf("[DEPLOYMENT] Node ID  : %"PRId16"\n", node_id);

  printf("[DEPLOYMENT] IEEE ADDR: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
         ieee_addr[0], ieee_addr[1],
         ieee_addr[2], ieee_addr[3],
         ieee_addr[4], ieee_addr[5],
         ieee_addr[6], ieee_addr[7]);
}

