TARGET ?= evb1000
TESTBED ?= mytestbed-evb1000

CONTIKI_PROJECT = glossy_test
all: $(CONTIKI_PROJECT)

# use deployment
PROJECTDIRS += ../deployment ../deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"

# include Glossy and Crystal
UWB_WITH_GLOSSY = 1

UWB_CONTIKI=../..
include $(UWB_CONTIKI)/Makefile.uwb
