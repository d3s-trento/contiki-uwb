CONTIKI_PROJECT = connectivity_test

# disable the stacks
CONTIKI_WITH_RIME = 0
CONTIKI_WITH_IPV6 = 0

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

CFLAGS+=-DNRF_SHOW_RESETREASON

all: $(CONTIKI_PROJECT)

UWB_CONTIKI=../../
include $(UWB_CONTIKI)/Makefile.uwb
