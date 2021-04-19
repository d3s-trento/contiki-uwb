TARGET ?= evb1000
TESTBED ?= mytestbed-evb1000

CONTIKI_PROJECT = crystal_test
all: $(CONTIKI_PROJECT)

# Deployment files
PROJECTDIRS += ../deployment ../deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c node-map.c

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

# include Glossy and Crystal
UWB_WITH_GLOSSY = 1

# print configuration macros at compile time
UWB_CONTIKI_PRINT_DEF = 1

UWB_CONTIKI=../..
include $(UWB_CONTIKI)/Makefile.uwb
