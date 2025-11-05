#include <string.h>
#include <inttypes.h>
#include <stdio.h>

#include <stdint.h>

#include "sys/node-id.h"
#include "net/netstack.h"
#include "deployment.h"

#if LINKADDR_SIZE > IEEE_ADDR_LEN
#error Wrong address sizes
#endif

unsigned short int node_id = 0;

/* Number of nodes in the deployment table */
extern const uint16_t deployment_num_nodes;

/* Deployment table */
extern const struct id_addr deployment_id_addr_list[];

uint8_t deployment_set_node_id_ieee_addr(void)
{
  /*
   * Read the 64 bit address stored in the Contiki netstack.
   * This assumes the field to be already set.
   */
  uint8_t ieee_addr[IEEE_ADDR_LEN];
  NETSTACK_RADIO.get_object(RADIO_PARAM_64BIT_ADDR, ieee_addr, IEEE_ADDR_LEN);

  for (int i=0; i<deployment_num_nodes; i++) {
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

  printf("[DEPLOYMENT] Node ID: %u, ", node_id);
  printf("IEEE addr: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
         ieee_addr[0], ieee_addr[1],
         ieee_addr[2], ieee_addr[3],
         ieee_addr[4], ieee_addr[5],
         ieee_addr[6], ieee_addr[7]);
}

bool deployment_get_addr_by_id(uint16_t node_id, linkaddr_t* addr) {
  for (uint16_t i=0; i<deployment_num_nodes; i++) {
    if (deployment_id_addr_list[i].id == node_id) {
      // copy all 8 bytes if long addresses are used or only the last two bytes
      // for short addresses.
      memcpy(addr, &deployment_id_addr_list[i].ieee_addr + IEEE_ADDR_LEN - LINKADDR_SIZE, LINKADDR_SIZE);
      return true;
    }
  }
  return false;
}
