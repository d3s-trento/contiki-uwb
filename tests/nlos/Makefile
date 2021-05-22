CONTIKI_PROJECT = test_nlos
all: $(CONTIKI_PROJECT)

CFLAGS += -DNRF_SHOW_RESETREASON

# set to 0 to use slow serial output
#PRINTF_OVER_RTT = 1

# enable softdevice
NRF52_SOFTDEVICE = 132

# Enable Rime
CONTIKI_WITH_IPV4 = 0
CONTIKI_WITH_IPV6 = 0
CONTIKI_WITH_RIME = 1

UWB_CONTIKI ?= ../..
include $(UWB_CONTIKI)/Makefile.uwb
