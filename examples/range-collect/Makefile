CONTIKI_PROJECT = collect-test
all: $(CONTIKI_PROJECT)

# Add EVB1000 platform folder
TARGETDIRS += ../../platform

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

# No network stack for the moment
CONTIKI_WITH_IPV4 = 0
CONTIKI_WITH_IPV6 = 0
CONTIKI_WITH_RIME = 1

CONTIKI ?= ../../../contiki
include $(CONTIKI)/Makefile.include
