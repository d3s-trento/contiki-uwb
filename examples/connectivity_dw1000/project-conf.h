#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#undef NETSTACK_NETWORK
#define NETSTACK_NETWORK connectivity_net

//DW1000_RXOFF_WHILE_PROCESSING is required to process packet while radio is off
//otherwise radio is switched on immediately and diagnostic information is lost
#define DW1000_RXOFF_WHILE_PROCESSING 1

//#define UART_CONF_BAUDRATE NRF5_SERIAL_BAUDRATE_1000000

#define LINKADDR_CONF_SIZE 8

#endif /* PROJECT_CONF_H_ */
