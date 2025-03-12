
#include "contiki.h"
#include "lib/random.h"
#include "leds.h"
#include "net/netstack.h"
#include <stdio.h>
#include "dw1000.h"
#include "dw1000-config.h"
#include "dw1000-ranging.h"
#include "core/net/linkaddr.h"
#include "serial-line.h"
#include "net/packetbuf.h"

#define RANGING_TIMEOUT (CLOCK_SECOND / 10)

const char major_version = 0;
const char minor_version = 0;

linkaddr_t linkaddr_broadcast;

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
  uint32_t password;
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

#if !defined(DW1000_RXOFF_WHILE_PROCESSING) || !DW1000_RXOFF_WHILE_PROCESSING
#error Connectivity test require DW1000_RXOFF_WHILE_PROCESSING defined true
#endif

static void
received()
{
  struct header header;
  dwt_rxdiag_t d;
  linkaddr_t sender;

  if (packetbuf_datalen() != sizeof(header)) {
    printf("error: packet of wrong length %d\n", packetbuf_datalen());
    return;
  }

  memcpy(&header, packetbuf_dataptr(), sizeof(header));

  if (header.password != 0xDEADBEEF) {
    printf("error: packet with wrong password %d\n", packetbuf_datalen());
    return;
  }

  dwt_readdiagnostics(&d);
  /* if (d == NULL) { */
  /*   printf("error: rxdiag not available\n"); */
  /*   return; */
  /* } */
  linkaddr_copy(&sender, packetbuf_addr(PACKETBUF_ADDR_SENDER));

  printf("received [");
  print_addr(&sender);
  printf("-");
  print_addr(&linkaddr_node_addr);
  printf("] seqn %lu, %u %u %u %u %u %u %u %u\n",
         header.seqn,
	 d.firstPathAmp1, d.firstPathAmp2, d.firstPathAmp3, d.maxGrowthCIR,
	 d.rxPreamCount, d.pacNonsat, d.maxNoise, d.stdNoise);

}


const struct network_driver connectivity_net = {
  "Connectivity",
  init,
  received,
};

static void
packet_sent(void *ptr, int status, int num_tx)
{
  /* switch(status) { */
  /* case MAC_TX_COLLISION: */
  /*   printf("collision after %d tx\n", num_tx); */
  /*   break;  */
  /* case MAC_TX_NOACK: */
  /*   printf("noack after %d tx\n", num_tx); */
  /*   break; */
  /* case MAC_TX_OK: */
  /*   printf("sent after %d tx\n", num_tx); */
  /*   break; */
  /* default: */
  /*   printf("error %d after %d tx\n", status, num_tx); */
  /* } */
}

void printDataRate(uint8_t dr, char *dst) {
  switch(dr) {
  case DWT_BR_110K:
    strcpy(dst, "110K");
    break;

  case DWT_BR_850K:
    strcpy(dst, "850K");
    break;

  case DWT_BR_6M8:
    strcpy(dst, "6.8M");
    break;

  default:
    strcpy(dst, "ERR");
    break;
  }
}

void printPrf(uint8_t prf, char *dst) {
  switch(prf) {
  case DWT_PRF_16M:
    strcpy(dst, "16M");
    break;

  case DWT_PRF_64M:
    strcpy(dst, "64M");
    break;

  default:
    strcpy(dst, "ERR");
    break;
  }
}

void printPac(uint8_t pac, char *dst) {
  switch(pac) {
  case DWT_PAC8:
    strcpy(dst, "8");
    break;

  case DWT_PAC16:
    strcpy(dst, "16");
    break;

  case DWT_PAC32:
    strcpy(dst, "32");
    break;

  case DWT_PAC64:
    strcpy(dst, "64");
    break;

  default:
    strcpy(dst, "ERR");
    break;
  }
}

void printPLen(uint8_t plen, char *dst) {
  switch(plen) {
  case DWT_PLEN_4096:
    strcpy(dst, "4096");
    break;

  case DWT_PLEN_2048:
    strcpy(dst, "2048");
    break;

  case DWT_PLEN_1536:
    strcpy(dst, "1536");
    break;

  case DWT_PLEN_1024:
    strcpy(dst, "1024");
    break;

  case DWT_PLEN_512:
    strcpy(dst, "512");
    break;

  case DWT_PLEN_256:
    strcpy(dst, "256");
    break;

  case DWT_PLEN_128:
    strcpy(dst, "128");
    break;

  case DWT_PLEN_64:
    strcpy(dst, "64");
    break;

  default:
    strcpy(dst, "ERR");
    break;
  }
}

void printPhrMode(uint8_t phrMode, char *dst) {
  switch(phrMode) {
  case DWT_PHRMODE_STD:
    strcpy(dst, "STD");
    break;

  case DWT_PHRMODE_EXT:
    strcpy(dst, "EXT");
    break;

  default:
    strcpy(dst, "ERR");
    break;
  }
}

void printPower( dwt_txconfig_t txconfig, int smart_mode, char *dst) {
  uint16_t reg, coarse, fine, phr, shr;
  if(!smart_mode) {
    reg = (txconfig.power & TX_POWER_TXPOWPHR_MASK) >> 8;
    coarse = (6-((reg & 0xE0) >> 5 )) * 30;//coarse setting
    fine = (reg & 0x1f) * 5; //fine setting
    phr = coarse + fine; // this value*10 to handle .5 step in integer math

    reg = (txconfig.power & TX_POWER_TXPOWSD_MASK) >> 16;
    coarse = (6-((reg & 0xE0) >> 5 )) * 30;//coarse setting
    fine = (reg & 0x1f) * 5; //fine setting
    shr = coarse + fine; // this value*10 to handle .5 step in integer math

    sprintf(dst, "Power: manual, %hu.%hu dB, %hu.%hu dB",
	    phr / 10, phr % 10,
	    shr / 10, shr % 10
	    );
  }
  else {
    sprintf(dst, "Power: smart, not yet implemented");
  }
}

void printConfig(dwt_config_t cfg, dwt_txconfig_t txconfig, int smart_mode) {
  char tmp[100];
  printf("DW1000 Radio Configuration: \n");
  printf("\t Channel: %hhu\n", cfg.chan);

  printPrf(cfg.prf, tmp);
  printf("\t PRF: %s\n", tmp);

  printPLen(cfg.txPreambLength, tmp);
  printf("\t PLEN: %s\n", tmp);

  printPac(cfg.rxPAC, tmp);
  printf("\t PAC Size: %s\n", tmp);

  printf("\t Preamble Code: %hhu %hhu\n", cfg.txCode, cfg.rxCode);
  printf("\t SFD: %hhu\n", cfg.nsSFD);

  printDataRate(cfg.dataRate, tmp);
  printf("\t Data Rate: %s\n", tmp);

  printPhrMode(cfg.phrMode, tmp);
  printf("\t PHR Mode: %s\n", tmp);
  printf("\t SFD Timeout: %hu\n", cfg.sfdTO);

  printPower(txconfig, smart_mode, tmp);
  printf("\t %s\n", tmp);
}

static void
parse_addr(char *buf, linkaddr_t *addr) {
  unsigned int i;
  unsigned short tmp;
  if(strcmp(buf, "BCAST") == 0) {
    for(i = 0; i < LINKADDR_SIZE; i++)
      addr->u8[i] = 0xff;
  } else {
    for(i = 0; i < LINKADDR_SIZE; i++) {
      sscanf(buf, "%2hx", &tmp); //%hhx is not supported
      addr->u8[i] = tmp;
      buf += 2;
      if(*buf == ':')
	buf +=1;
    }
  }
}

PROCESS_THREAD(cli_connectivity, ev, data)
{

  static struct etimer et;
  static uint16_t count_packets = 0;
  static struct header header = {0, 0xDEADBEEF};
  static uint8_t u8;
  static uint16_t u16, tmp16a, tmp16b;
  static int status;
  static char txtBuffer[100];

  // local cache of the radio configs
  static dwt_config_t radio_config;
  static dwt_txconfig_t txconfig;
  static bool smart_tx_power;

  PROCESS_BEGIN();

  for(u8 = 0; u8 < LINKADDR_SIZE; u8++)
    linkaddr_broadcast.u8[u8] = 0xff;

  radio_config = *dw1000_get_current_cfg();
  txconfig = *dw1000_get_current_tx_cfg();
  smart_tx_power = dw1000_is_smart_tx_enabled();

  //dw1000_print_cfg();

  while(1) {
    printf("> \n"); // \n is needed to avoid screwing up the output format
    PROCESS_WAIT_EVENT_UNTIL(ev == serial_line_event_message);
    //printf("dbg:'%s'\n", (char*)data);

    if(strncmp((char*)data, "HELLO", 5) == 0) {
      printf("HELLO I'm ");
      print_addr(&linkaddr_node_addr);
      printf("\n");

    } else if(strncmp((char*)data, "VERS", 4) == 0) {
      printf("version: %d.%d\n", major_version, minor_version);

    } else if(strncmp((char*)data, "SEND ", 5) == 0) {

      /* int err = */sscanf((char*)data, "SEND %[^,],%hu,%lu", txtBuffer,
		       &send_args.packets, &send_args.ipi);

      /* printf("dbg: err %d\n", err); */
      /* printf("dbg: txtbuffer: %s\n", txtBuffer); */
      /* printf("dbg: %p %p\n", &send_args.packets, &send_args.ipi); */
      /* printf("dbg: %hu %lu\n", send_args.packets, send_args.ipi); */

      parse_addr(txtBuffer, &send_args.dst);

      printf("have to send %hu packets to ", send_args.packets);
      print_addr(&send_args.dst);
      printf(" every %lu\n", send_args.ipi);

      if(linkaddr_cmp(&linkaddr_node_addr, &send_args.dst))
	printf("error i'm also recipient\n");
      else {
	printf("start test\n");
	count_packets = 0;

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

    } else if(strncmp((char*)data, "GET_CHANNEL", 11) == 0) {
      //getting channel
      printf("channel: %hu\n", radio_config.chan);

    } else if(strncmp((char*)data, "SET_CHANNEL", 11) == 0) {
      sscanf((char*)data, "SET_CHANNEL %hu\n", &u16);

      switch(u16) {
      case 1:
	txconfig.PGdly = TC_PGDELAY_CH1;
	radio_config.chan = u16 & 0xff;
	break;

      case 2:
	txconfig.PGdly = TC_PGDELAY_CH2;
	radio_config.chan = u16 & 0xff;
	break;

      case 3:
	txconfig.PGdly = TC_PGDELAY_CH3;
	radio_config.chan = u16 & 0xff;
	break;

      case 4:
	txconfig.PGdly = TC_PGDELAY_CH4;
	radio_config.chan = u16 & 0xff;
	break;

      case 5:
	txconfig.PGdly = TC_PGDELAY_CH5;
	radio_config.chan = u16 & 0xff;
	break;

      case 7:
	txconfig.PGdly = TC_PGDELAY_CH7;
	radio_config.chan = u16 & 0xff;
	break;

      default:
	printf("Wrong channel number\n");
	break;
      }

      printf("new channel: %hhu\n", radio_config.chan);

    } else if(strncmp((char*)data, "GET_PRF", 7) == 0) {
      char tmp[3];
      printPrf(radio_config.prf, tmp);
      printf("prf: %s\n", tmp);

    } else if(strncmp((char*)data, "SET_PRF", 7) == 0) {
      char tmp[3];
      sscanf((char*)data, "SET_PRF %s\n", tmp);

      if(strcmp(tmp, "16M") == 0) {
	radio_config.prf = DWT_PRF_16M;
	printf("new prf: 16M\n");

      } else if(strcmp(tmp, "64M") == 0) {
	radio_config.prf = DWT_PRF_64M;
	printf("new prf: 64M\n");
      } else
	printf("wrong PRF value\n");

    } else if(strncmp((char*)data, "GET_PLEN", 8) == 0) {
      char tmp[4];
      printPLen(radio_config.txPreambLength, tmp);
      printf("preamble len: %s\n", tmp);

    } else if(strncmp((char*)data, "SET_PLEN", 8) == 0) {
      sscanf((char*)data, "SET_PLEN %hu\n", &u16);

      switch(u16) {
      case 4096:
	radio_config.txPreambLength = DWT_PLEN_4096;
	printf("new preamble len: %hu\n", u16);
	break;

      case 2048:
	radio_config.txPreambLength = DWT_PLEN_2048;
	printf("new preamble len: %hu\n", u16);
	break;


      case 1536:
	radio_config.txPreambLength = DWT_PLEN_1536;
	printf("new preamble len: %hu\n", u16);
	break;

      case 1024:
	radio_config.txPreambLength = DWT_PLEN_1024;
	printf("new preamble len: %hu\n", u16);
	break;

      case 512:
	radio_config.txPreambLength = DWT_PLEN_512;
	printf("new preamble len: %hu\n", u16);
	break;

      case 256:
	radio_config.txPreambLength = DWT_PLEN_256;
	printf("new preamble len: %hu\n", u16);
	break;

      case 128:
	radio_config.txPreambLength = DWT_PLEN_128;
	printf("new preamble len: %hu\n", u16);
	break;

      case 64:
	radio_config.txPreambLength = DWT_PLEN_64;
	printf("new preamble len: %hu\n", u16);
	break;

      default:
	printf("wrong preamble len value\n");
      }

    } else if(strncmp((char*)data, "GET_RXPAC", 9) == 0) {
      char tmp[3];
      printPac(radio_config.rxPAC, tmp);
      printf("rxPAC: %s\n",  tmp);

    } else if(strncmp((char*)data, "SET_RXPAC", 9) == 0) {
      sscanf((char*)data, "SET_RXPAC %hhu\n", &u8);

      switch(u8) {
      case 8:
	radio_config.rxPAC = DWT_PAC8;
	printf("new rxPAC: %hhu\n", u8);
	break;

      case 16:
	radio_config.rxPAC = DWT_PAC16;
	printf("new rxPAC: %hhu\n", u8);
	break;

      case 32:
	radio_config.rxPAC = DWT_PAC32;
	printf("new rxPAC: %hhu\n", u8);
	break;

      case 64:
	radio_config.rxPAC = DWT_PAC64;
	printf("new rxPAC: %hhu\n", u8);
	break;

      default:
	printf("worng rxpac value\n");
	break;
      }

    } else if(strncmp((char*)data, "GET_TXCODE", 10) == 0) {
      printf("txCode: %hhu\n", radio_config.txCode);

    } else if(strncmp((char*)data, "SET_TXCODE", 10) == 0) {
      sscanf((char*)data, "SET_TXCODE %hu\n", &u16);

      radio_config.txCode = u16 & 0xff;
      printf("new txCode: %hhu\n", radio_config.txCode);

    } else if(strncmp((char*)data, "GET_RXCODE", 10) == 0) {
      printf("rxcode: %hhu\n", radio_config.rxCode);

    } else if(strncmp((char*)data, "SET_RXCODE", 10) == 0) {
      sscanf((char*)data, "SET_RXCODE %hu\n", &u16);

      radio_config.rxCode = u16 & 0xff;
      printf("new rxcode: %hhu\n", radio_config.rxCode);

    } else if(strncmp((char*)data, "GET_NSSFD", 9) == 0) {
      printf("nsSFD: %hhu\n", radio_config.nsSFD);

    } else if(strncmp((char*)data, "SET_NSSFD", 9) == 0) {
      sscanf((char*)data, "SET_NSSFD %hhu\n", &u8);

      radio_config.nsSFD = u8;
      printf("new nsSFD: %hhu\n", radio_config.nsSFD);

    } else if(strncmp((char*)data, "GET_DATARATE", 12) == 0) {
      char tmp[4];
      printDataRate(radio_config.dataRate, tmp);
      printf("dataRate: %s\n", tmp);

    } else if(strncmp((char*)data, "SET_DATARATE", 12) == 0) {
      char tmp[4];

      sscanf((char*)data, "SET_DATARATE %s\n", tmp);

      if(strcmp(tmp, "110K") == 0) {
	radio_config.dataRate = DWT_BR_110K;
	printf("new data rate 110k\n");
      } else if(strcmp(tmp, "850K") == 0) {
	radio_config.dataRate = DWT_BR_850K;
	printf("new data rate 850k\n");
      } else if(strcmp(tmp, "6M8") == 0) {
	radio_config.dataRate = DWT_BR_6M8;
	printf("new data rate 6.8M\n");
      } else
	printf("wrong data rate value\n");

    } else if(strncmp((char*)data, "GET_PHRMODE", 11) == 0) {
      char tmp[3];
      printPhrMode(radio_config.phrMode, tmp);
      printf("phrMode: %s\n", tmp);

    } else if(strncmp((char*)data, "SET_PHRMODE", 11) == 0) {
      char tmp[3];
      sscanf((char*)data, "SET_PHRMODE %s\n", tmp);

      if(strcmp(tmp, "STD") == 0) {
	radio_config.phrMode = DWT_PHRMODE_STD;
	printf("new phrMode: STD\n");
      } else if(strcmp(tmp, "EXT") == 0) {
	radio_config.phrMode = DWT_PHRMODE_EXT;
	printf("new phrMode: EXT\n");
      } else
	printf("wrong PHR mode value\n");

    } else if(strncmp((char*)data, "GET_SFDTO", 9) == 0) {
      printf("sfdTO: %hu\n", radio_config.sfdTO);

    } else if(strncmp((char*)data, "SET_SFDTO", 9) == 0) {
      sscanf((char*)data, "SET_SFDTO %hu\n", &u16);

      radio_config.sfdTO = u16;
      printf("new sfdTO: %hu\n", radio_config.sfdTO);

    } else if(strncmp((char*)data, "GET_POWER", 9) == 0) {
	printPower(txconfig, smart_tx_power, txtBuffer);
	printf("%s\n", txtBuffer);

    } else if(strncmp((char*)data, "SET_POWER", 9) == 0) {

      sscanf((char*)data, "SET_POWER %hu", &u16);

      if(u16 == 0) {
	sscanf((char*)data, "SET_POWER 0,%hu", &u16);

	smart_tx_power = 0;
	printf("Set manual tx power %u\n", u16);

	tmp16a = (u16/30);//coarse setting
	tmp16a = (tmp16a < 6) ? (6 - tmp16a) : 0;
	tmp16b = (u16 - (6 - tmp16a)*30)/5;
	if(tmp16b <= 0x1f) {
	  //printf("expected %hu %hu", tmp16a, tmp16b);
	  u8 = ((tmp16a & 0x7) << 5) + ((tmp16b) & 0x1f);
	  //printf("register: %02x\n", u8);
	  txconfig.power = (u8 << 24) + (u8 << 16) + (u8 << 8) + u8; //set all four byte, only two are used
	  printf("result: %08lx\n", txconfig.power);
	} else {
	  printf("Out of bound value, maximum is 33.5 dB\n");
	}

	printPower(txconfig, smart_tx_power, txtBuffer);
	printf("%s\n", txtBuffer);

      } else if(u16 == 1) {
	printf("Set smart tx power\n");
	smart_tx_power = 1;
	sscanf((char*)data, "SET_POWER 1,%hu", &u16);
	printf("NOT IMPLEMENTED, %hu\n", u16);

      } else if (u16 == 2) {
	printf("Set raw register for manual tx power\n");
	smart_tx_power = 0;
	sscanf((char*)data, "SET_POWER 2,%8lx", &txconfig.power);
	printPower(txconfig, smart_tx_power, txtBuffer);
	printf("%s\n", txtBuffer);

      } else
	printf("Wrong value for power mode\n");



    } else if(strncmp((char*)data, "COMMIT", 6) == 0) {
      /* set settings */
      dw1000_configure(&radio_config);
      dw1000_configure_tx(&txconfig, smart_tx_power);

      /* reload local cache */
      radio_config = *dw1000_get_current_cfg();
      txconfig = *dw1000_get_current_tx_cfg();
      smart_tx_power = dw1000_is_smart_tx_enabled();

      dw1000_ranging_init(); // need to reinitialise the ranging module
      NETSTACK_RADIO.on();   // turn the radio ON after reconfiguring
      printConfig(radio_config, txconfig, smart_tx_power);

    } else if(strncmp((char*)data, "ROLLBACK", 8) == 0) {
      radio_config = *dw1000_get_current_cfg();
      txconfig = *dw1000_get_current_tx_cfg();
      smart_tx_power = dw1000_is_smart_tx_enabled();

      printConfig(radio_config, txconfig, smart_tx_power);

    } else if(strncmp((char*)data, "GET_CONFIGSTATUS", 16) == 0) {

      dwt_config_t tmp_radio_config;
      dwt_txconfig_t tmp_txconfig;
      bool tmp_smart_tx_power;

      tmp_radio_config = *dw1000_get_current_cfg();
      tmp_txconfig = *dw1000_get_current_tx_cfg();
      tmp_smart_tx_power = dw1000_is_smart_tx_enabled();

      //printConfig(radio_config, txconfig, smart_tx_power);
      //printConfig(tmp_radio_config, tmp_txconfig, tmp_smart_tx_power);

      if(memcmp(&radio_config, &tmp_radio_config, sizeof(radio_config)) == 0 &&
	 memcmp(&txconfig, &tmp_txconfig, sizeof(txconfig)) == 0 &&
	 smart_tx_power == tmp_smart_tx_power)

	printf("commited\n");
      else
	printf("NOT commited\n");

    } else if(strncmp((char*)data, "GET_CONFIG", 10) == 0) {
      printConfig(radio_config, txconfig, 0);

    } else if(strncmp((char*)data, "RANGE", 5) == 0) {

      u8 = sscanf((char*)data, "RANGE %[^,],%hu,%lu\n", txtBuffer,
	     &send_args.packets, &send_args.ipi);

      //printf("dbg: %d %s\n", u8, txtBuffer);

      parse_addr(txtBuffer, &send_args.dst);

      printf("have to range with ");
      print_addr(&send_args.dst);
      printf(" %hu times every %lu\n", send_args.packets, send_args.ipi);

      if(linkaddr_cmp(&linkaddr_node_addr, &send_args.dst)) {
	printf("error i'm also recipient\n");
      } else if(linkaddr_cmp(&linkaddr_broadcast, &send_args.dst)) {
	printf("error: ranging with broadcast isn't implemented yet\n");
      } else {
 	printf("start test\n");
	count_packets = 0;

	while(count_packets < send_args.packets) {
	  status = range_with(&send_args.dst, DW1000_RNG_DS);
	  if (!status) {
	    printf("range request failed\n");
	  }
	  else {
	    etimer_set(&et, RANGING_TIMEOUT);
	    PROCESS_YIELD_UNTIL((ev == ranging_event || etimer_expired(&et)));
	    if (etimer_expired(&et)) {
	      printf("range timeout\n");
	    }
	    else if (((ranging_data_t*)data)->status) {
	      ranging_data_t* d = data;
	      printf("range [");
              print_addr(&send_args.dst);
              printf("-");
              print_addr(&linkaddr_node_addr);
              printf("]: %d bias %d\n",
		     (int)(d->raw_distance*100),
		     (int)(d->distance*100));
	    }
	    else
	      printf("range failed %d\n", ((ranging_data_t*)data)->status);

	    count_packets++;
	    if(count_packets < send_args.packets && send_args.ipi > 0) {
	      etimer_set(&et, send_args.ipi * (CLOCK_SECOND /1000));
	      PROCESS_WAIT_UNTIL(etimer_expired(&et));
	    }
	  }
	}
	printf("end test\n");
      }

    } else {
      printf("error unrecognized '%s'\n", (char*)data);
    }

    //printConfig(radio_config);
  }

  PROCESS_END();
}
