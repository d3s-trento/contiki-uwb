CONTIKI_PROJECT = example-unicast

all: example-unicast

#enable softdevice
NRF52_SOFTDEVICE = 132

CONTIKI_WITH_RIME = 1
DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

UWB_CONTIKI ?= ../..
include $(UWB_CONTIKI)/Makefile.uwb
