powers = [(0, 0x9a9a9a9a)]
radio_cfg = 6
sinks = [9]
testbed = "unitn"
start_epoch = 1
active_epochs = 200
full_epochs = 15
#num_senderss = [0,1,2,5,10,20]
num_senderss = [1,5]

glossy_versions = ["std","txo"]

period = 0.8

n_tx_s = 3
dur_s  = 10

n_tx_t = 3
dur_t  = 10

n_tx_a = 3
dur_a  = 10

payload = 2

n_emptys = [(2,2,4,6)]

nodes=[1,2,  4,5,6,7,8,9,10,11,12,13,14,15,16,17]

#chmap = "nomap"
#boot_chop = "hop3"
chmap = "nohop"
boot_chop = "nohop"

logging = True
seed = 123


ts_init = "asap"
duration = 900
