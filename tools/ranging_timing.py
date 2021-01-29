from uwb_txtime import *

CONFIG = DwtConfig(DWT_PRF_64M, DWT_BR_6M8, DWT_PLEN_128)
FRAME_LEN = 50
RESP_DELAY = 300
RXGUARD = 50


rmonly = CONFIG.estimate_tx_time(FRAME_LEN, True)/1000
full   = CONFIG.estimate_tx_time(FRAME_LEN, False)/1000


# request                             response
# [ preamble | data ]         ........[ preamble | data ]
#          rmarker    sleep    listen


rxaftertx = RESP_DELAY - full - RXGUARD
rxtimeout = RXGUARD + full

print(f"Full frame duration: {full}, SHR: {rmonly}, PHR+PSDU: {full-rmonly}")
print(f"Response delay: {RESP_DELAY}, RX guard: {RXGUARD}, rx_after_tx: {rxaftertx}, rxto: {rxtimeout}")



