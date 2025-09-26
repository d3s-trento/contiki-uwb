# TX power setting (smart_tx_enabled, tx_power)
powers = [(0, 0x9a9a9a9a)]

# radio configuration set to use (see the project-conf.h)
radio_cfg = 6

# sink nodes
sinks = [119]

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
num_senderss = [0,1,5]
#num_senderss = [60]

# the Glossy variants to use
#crystal_versions = ["no", "aggr", "cons", "simple"]
crystal_versions = ["no", "simple"]

# Epoch duration
period = 0.8

# N parameters and flood durations for S, T and A slots
n_tx_s = 2
dur_s  = 10

n_tx_t = 2
dur_t  = 10

n_tx_a = 2
dur_a  = 10

# application payload size
payload = 2

# parameters defining the termination condition (it's recommended to keep the defaults)
n_emptys = [(2,2,4,6)]

# nodes to participate in the test
nodes = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 24,
         25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 100, 101, 102, 103, 104,
         105, 106, 107, 109, 110, 111, 152, 153, 150, 151, 149, 148, 147, 138,
         137, 136, 135, 134, 133, 131, 132, 128, 127, 130, 129, 125, 126, 124,
         123, 122, 121, 118, 119, 145, 146, 143, 144, 141, 142, 139, 140]
# =[1,2,4,5,6,7,8,9,10,11,12,13,14,15,16,17]

all_senders = [1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 14, 15, 16, 17, 18, 19, 24, 25, 27,
               28, 29, 30, 31, 32, 33, 35, 36, 100, 102, 104, 105, 106, 109,
               110, 111, 121, 122, 123, 124, 125, 127, 128, 129, 130, 132,
               133, 134, 135, 136, 137, 138, 139, 140, 141, 143, 144, 145, 148,
               149, 151, 153]

# channel hopping (currently not supported on UWB, so keep the following)
chmap = "nohop"
boot_chop = "nohop"

# Enable logging
logging = True

# Random seed to generate the sender lists
seed = 123

# UniTn testbed parameters
ts_init = "asap"
duration = 720

