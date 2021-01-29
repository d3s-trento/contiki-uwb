class NbConfig:

    def __init__(self):
        self.phr_len = 1      # B
        self.preamble_len = 5 # B
        self.data_rate= 250 #k

    def get_params_mnemonics(self):
        return "%sk" % self.data_rate, self.preamble_len

    def estimate_tx_time(self, framelength, only_rmarker=False):
        return (framelength + self.phr_len + self.preamble_len) * 32 * 1e3     #ns


