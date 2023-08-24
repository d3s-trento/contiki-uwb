
#include "contiki.h"
#include "lib/random.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "core/net/linkaddr.h"
#include "serial-line.h"
#include "net/packetbuf.h"
#include "dev/cc2538-rf.h"

const char major_version = 0;
const char minor_version = 0;

const linkaddr_t linkaddr_broadcast = {{0xff, 0xff}};

PROCESS(cli_connectivity, "CLI of Connectivity Test");
AUTOSTART_PROCESSES(&cli_connectivity);

typedef struct start_send_args_t {
  linkaddr_t dst;
  uint32_t ipi;
  uint16_t packets;
} start_send_args_t;

static start_send_args_t send_args;

struct header {
  uint32_t seqn;
};

void
print_addr(linkaddr_t *addr) {
  int i;
  printf("%02x", addr->u8[0]);
  for(i=1; i < LINKADDR_SIZE; i++)
    printf(":%02x", addr->u8[i]);
}

static void
init()
{
}

static void
received()
{
  struct header header;
  linkaddr_t sender;
  int8_t rssi;
  uint8_t lqi;

  if (packetbuf_datalen() != sizeof(header)) {
    printf("error: packet of wrong length %d\n", packetbuf_datalen());
    return;
  }

  linkaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));
  memcpy(&header, packetbuf_dataptr(), sizeof(header));
  rssi = (int8_t)packetbuf_attr(PACKETBUF_ATTR_RSSI);
  lqi = packetbuf_attr(PACKETBUF_ATTR_LINK_QUALITY);


  printf("received [");
  print_addr(&sender);
  printf("-");
  print_addr(&linkaddr_node_addr);
  printf("] seqn %lu rssi %d lqi %d\n", header.seqn, rssi, lqi);

}

static void
packet_sent(void *ptr, int status, int num_tx)
{
}

const struct network_driver connectivity_net = {
						"Connectivity",
						init,
						received,
};

typedef struct radio_config_t {
  radio_value_t channel, power;
} radio_config_t;

void
print_config(radio_config_t *config) {
  printf("Radio configuration.\n");
  printf("\tchannel: %d\n", config->channel);
  printf("\tpower: %d\n", config->power);
  printf("\n");
}

PROCESS_THREAD(cli_connectivity, ev, data)
{

  static struct etimer et;
  static uint16_t count_packets = 0;
  static struct header header = {0};
  static uint8_t u8;
  static uint16_t u16, tmp16a, tmp16b;

  static radio_config_t radio_config, radio_config_tmp;

  PROCESS_BEGIN();

  cc2538_rf_driver.get_value(RADIO_PARAM_CHANNEL, &radio_config.channel);
  cc2538_rf_driver.get_value(RADIO_PARAM_TXPOWER, &radio_config.power);

  radio_config_tmp = radio_config;

  while(1) {
    printf("> \n"); // \n is needed to avoid screwing up the output format
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message && data != NULL);
    printf("dbg:'%s'\n", (char*)data);

    if(strncmp((char*)data, "HELLO", 5) == 0) {
      printf("HELLO I'm ");
      print_addr(&linkaddr_node_addr);
      printf("\n");

    } else if(strncmp((char*)data, "VERS", 4) == 0) {
      printf("version: %d.%d\n", major_version, minor_version);

    } else if(strncmp((char*)data, "GET_CONFIGSTATUS", 16) == 0) {
      if(memcmp(&radio_config, &radio_config_tmp, sizeof(radio_config)) == 0)
	printf("commited\n");
      else
	printf("NOT commited\n");

    } else if(strncmp((char*)data, "GET_CONFIG", 10) == 0) {
      print_config(&radio_config_tmp);

    } else if(strncmp((char*)data, "COMMIT", 6) == 0) {
      if(cc2538_rf_driver.set_value(RADIO_PARAM_CHANNEL, radio_config_tmp.channel) == RADIO_RESULT_OK) {
	if(cc2538_rf_driver.set_value(RADIO_PARAM_TXPOWER, radio_config_tmp.power) == RADIO_RESULT_OK)
	  radio_config = radio_config_tmp;
	else {
	  //error setting txpower
	  //rollback channel
	  cc2538_rf_driver.set_value(RADIO_PARAM_CHANNEL, radio_config.channel);
	}
      }

      if(memcmp(&radio_config, &radio_config_tmp, sizeof(radio_config_t)) != 0)
	printf("Error applying configuration\n");
      else
	printf("Committed\n");
      print_config(&radio_config);

    } else if(strncmp((char*)data, "ROLLBACK", 8) == 0) {
      radio_config_tmp = radio_config;

    } else if(strncmp((char*)data, "SEND ", 5) == 0) {
      char buf[LINKADDR_SIZE*2 +1];

      /* int err =  */sscanf((char*)data, "SEND %[^,],%hu,%lu", buf,
			     &send_args.packets, &send_args.ipi);

      if(strcmp(buf, "BCAST") == 0)
	for(int i = 0; i < LINKADDR_SIZE; i++)
	  send_args.dst.u8[i] = 0xFF;
      else
	for(int i = 0; i < LINKADDR_SIZE; i++)
	  sscanf(buf + i*2, "%2hhx", send_args.dst.u8 + i);

      /* print_addr(&send_args.dst); */
      /* printf("\n"); */

      printf("have to send %hu packets to ", send_args.packets);
      print_addr(&send_args.dst);
      printf(" every %lu\n", send_args.ipi);

      if(linkaddr_cmp(&linkaddr_node_addr, &send_args.dst))
    	printf("error i'm also recipient\n");
      else {
    	printf("start test\n");
    	count_packets = 0;
	header.seqn = 0;

    	while(count_packets < send_args.packets) {
    	  packetbuf_clear();
    	  packetbuf_copyfrom(&header, sizeof(header));

    	  NETSTACK_LLSEC.send(packet_sent, NULL);

    	  count_packets++;
	  printf("sent [");
	  print_addr(&linkaddr_node_addr);
	  printf("] seqn %lu\n", header.seqn);
          header.seqn ++;

    	  if(count_packets < send_args.packets && send_args.ipi > 0) {
    	    if(count_packets == 1)
    	      etimer_set(&et, (send_args.ipi * CLOCK_SECOND) /1000);
    	    else
    	      etimer_reset(&et);
    	    PROCESS_WAIT_UNTIL(etimer_expired(&et));
    	  }
    	}

    	printf("end test\n");
      }

    } else if (strncmp((char*)data, "GET_CHANNEL", 11) == 0) {
      printf("Radio Channel: %d\n", radio_config_tmp.channel);

    } else if (strncmp((char*)data, "SET_CHANNEL", 11) == 0) {
      sscanf((char*)data, "SET_CHANNEL %hhu\n", &u8);
      if(u8 <  CC2538_RF_CHANNEL_MIN || u8 > CC2538_RF_CHANNEL_MAX)
	printf("Error: not valid channel number\n");
      else {
	radio_config_tmp.channel = u8;
      }

    } else if(strncmp((char*)data, "GET_POWER", 9) == 0) {
      printf("power: %d\n", radio_config_tmp.power);

    } else if(strncmp((char*)data, "SET_POWER", 9) == 0) {
      radio_value_t i8;
      sscanf((char*)data, "SET_POWER %d\n", &i8);
      radio_config_tmp.power = i8;
    } else {
      printf("error unrecognized '%s'\n", (char*)data);
    }

    //printConfig(radio_config);
  }

  PROCESS_END();
}
