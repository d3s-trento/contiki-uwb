#ifndef DEPLOYMENT_H
#define DEPLOYMENT_H

#include <inttypes.h>
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

#endif /* DEPLOYMENT_H */
