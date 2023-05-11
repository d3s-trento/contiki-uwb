TARGET ?= evb1000
TESTBED ?= mytestbed-evb1000 # define the correct testbed to be used

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

CONTIKI_PROJECT = glossy
all: $(CONTIKI_PROJECT)

# include Glossy and Crystal
UWB_WITH_TSM_GLOSSY = 1
UWB_WITH_TSM = 1

# print configuration macros at compile time
UWB_CONTIKI_PRINT_DEF = 1

UWB_CONTIKI ?=../..

PROJECTDIRS += $(UWB_CONTIKI)/examples/deployment ../deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c node-map.c

include $(UWB_CONTIKI)/Makefile.uwb
