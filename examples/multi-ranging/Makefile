CONTIKI_PROJECT = multi-rng
all: $(CONTIKI_PROJECT)

CFLAGS += -DNRF_SHOW_RESETREASON

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

# set to 0 to use slow serial output
PRINTF_OVER_RTT = 1

# Enable Rime
CONTIKI_WITH_IPV4 = 0
CONTIKI_WITH_IPV6 = 0
CONTIKI_WITH_RIME = 1

UWB_CONTIKI ?= ../..
include $(UWB_CONTIKI)/Makefile.uwb
