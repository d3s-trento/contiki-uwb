TARGET ?= evb1000
TESTBED ?= mytestbed-evb1000

SIMGEN ?= false

ifeq ($(SIMGEN),true)
$(info "No simgen is being used")

ifdef SINK_ID
$(info "SINK_ID: 									$(SINK_ID)")
CFLAGS += -DSINK_ID=$(SINK_ID)
else 
$(error "No sink set")
endif

ifdef SINK_RADIUS
$(info "SINK_RADIUS: 									$(SINK_RADIUS)")
CFLAGS += -DSINK_RADIUS=$(SINK_RADIUS)
else
$(error "No sink radius set")
endif

ifdef NODES_DEPLOYED
$(info "NODES_DEPLOYED: 								$(NODES_DEPLOYED)")
CFLAGS += -DNODES_DEPLOYED=$(NODES_DEPLOYED)
else 
$(error "No deployment nodes set")
endif

# # Removed parameter (code kept for possible later introduction)
# ifdef WEAVER_MAX_EPOCH_SLOT
# $(info "WEAVER_MAX_EPOCH_SLOT: 	$(WEAVER_MAX_EPOCH_SLOT)")
# CFLAGS += -DWEAVER_MAX_EPOCH_SLOT=$(WEAVER_MAX_EPOCH_SLOT)
# else 
# $(error "No max slot set")
# endif

ifdef WEAVER_N_ORIGINATORS
$(info "N ORIGINATORS: 									$(WEAVER_N_ORIGINATORS)")
CFLAGS += -DWEAVER_N_ORIGINATORS=$(WEAVER_N_ORIGINATORS)
else
$(error "No originators set")
endif

$(info "ORIGINATORS_TABLE: 							$(WEAVER_ORIGINATORS_TABLE)")
CFLAGS += -DWEAVER_ORIGINATORS_TABLE=$(WEAVER_ORIGINATORS_TABLE)

ifdef WEAVER_APP_START_EPOCH
$(info "START EPOCH: 										$(WEAVER_APP_START_EPOCH)")
CFLAGS += -DWEAVER_APP_START_EPOCH=$(WEAVER_APP_START_EPOCH)
else
$(error "No start epoch defined")
endif

ifdef WEAVER_EPOCHS_PER_CYCLE
$(info "WEAVER_EPOCHS_PER_CYCLE: 	$(WEAVER_EPOCHS_PER_CYCLE)")
CFLAGS += -DWEAVER_EPOCHS_PER_CYCLE=$(WEAVER_EPOCHS_PER_CYCLE)
else
$(error "Number of epochs per cycle not defined")
endif

ifdef EXTRA_PAYLOAD_LEN
$(info "EXTRA_PAYLOAD_LEN: 									$(EXTRA_PAYLOAD_LEN)")
CFLAGS += -DEXTRA_PAYLOAD_LEN=$(EXTRA_PAYLOAD_LEN)
endif

else
$(info "No simgen is being used")
endif # SIMGEN


#CONTIKI_PROJECT = trex-test-pt trex-nd tsm-test
CONTIKI_PROJECT = weaver
all: $(CONTIKI_PROJECT)

DEFINES+=PROJECT_CONF_H=\"project-conf.h\"


# include the TSM kernel
UWB_WITH_TSM = 1

# print configuration macros at compile time
UWB_CONTIKI_PRINT_DEF = 0


# include rr_table & utility
PROJECT_SOURCEFILES += rrtable.c
PROJECT_SOURCEFILES += weaver-utility.c weaver-log.c

UWB_CONTIKI=../..

# use deployment
PROJECTDIRS += $(UWB_CONTIKI)/examples/deployment $(UWB_CONTIKI)/examples/deployment/$(TESTBED)
PROJECT_SOURCEFILES += deployment.c

include $(UWB_CONTIKI)/Makefile.uwb
