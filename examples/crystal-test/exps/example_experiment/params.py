# TX power setting (smart_tx_enabled, tx_power)
powers = [(0, 0x9a9a9a9a)]

# radio configuration set to use (see the project-conf.h)
radio_cfg = 6

# sink nodes
sinks = [9]

# testbed to generate tests for
testbed = "unitn"

# the epoch when the tests starts sending data packets
start_epoch = 1

# number of epochs in the precompiled send table (it is repeated cyclically 
# if the test contains more epocs than what is set)
active_epochs = 200

# force several first epochs to send as many ACKs as possible to speed-up
# bootstrapping
full_epochs = 15

# number of concurrent senders (the U parameter in the research paper)
num_senderss = [1,5]

# the Glossy variants to use
glossy_versions = ["std","txo"]

# Epoch duration
period = 0.8

# N parameters and flood durations for S, T and A slots
n_tx_s = 3
dur_s  = 10

n_tx_t = 3
dur_t  = 10

n_tx_a = 3
dur_a  = 10


# application payload size
payload = 2

# parameters defining the termination condition (it's recommended to keep the defaults)
n_emptys = [(2,2,4,6)]

# nodes to participate in the test
nodes=[1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17]

# channel hopping (currently not supported on UWB, so keep the following)
chmap = "nohop"
boot_chop = "nohop"

# Enable logging
logging = True

# Random seed to generate the sender lists
seed = 123

# UniTn testbed parameters
ts_init = "asap"
duration = 900
