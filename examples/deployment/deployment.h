#ifndef DEPLOYMENT_H
#define DEPLOYMENT_H

#include <inttypes.h>
#include <stdbool.h>
#include "net/netstack.h"
#include "sys/node-id.h"

#define IEEE_ADDR_LEN 8

struct id_addr {
  uint16_t id;
  uint8_t ieee_addr[IEEE_ADDR_LEN];
};

/* Assign node ID based on the deployment table */
uint8_t deployment_set_node_id_ieee_addr(void);

/* Get the number of nodes in the deployment table */
uint16_t deployment_get_n_nodes(void);

/* Print the current node ID and extended address 
 *
 * Note: call deployment_set_node_id_ieee_addr() before
 */
void deployment_print_id_info(void);

/* Get the node address from its ID.
 *
 * If node_id is found in the table, copies the address to addr and
 * returns true. Otherwise returns false.
 */
bool deployment_get_addr_by_id(uint16_t node_id, linkaddr_t* addr);

#endif /* DEPLOYMENT_H */
