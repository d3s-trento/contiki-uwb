TARGET ?= evb1000
TESTBED ?= mytestbed-evb1000 # define the correct testbed to be used

CONTIKI_PROJECT = trexd-test trexd-nd tsm-test tsm-nd
all: $(CONTIKI_PROJECT)

#DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

UWB_WITH_TSM = 1

# print configuration macros at compile time
UWB_CONTIKI_PRINT_DEF = 1

UWB_CONTIKI=../..

PROJECTDIRS += $(UWB_CONTIKI)/examples/deployment $(UWB_CONTIKI)/examples/deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c

include $(UWB_CONTIKI)/Makefile.uwb
