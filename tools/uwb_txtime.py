#!/usr/bin/env python3

DATA_BLOCK_SIZE  = 330
REED_SOLOM_BITS  = 48

DWT_PRF_16M = 1
DWT_PRF_64M = 2

DWT_BR_110K = 0
DWT_BR_850K = 1
DWT_BR_6M8  = 2

DWT_PLEN_64  = 0x04
DWT_PLEN_128 = 0x14
DWT_PLEN_256 = 0x24
DWT_PLEN_512 = 0x34
DWT_PLEN_1024 = 0x08
DWT_PLEN_1536 = 0x18
DWT_PLEN_2048 = 0x28
DWT_PLEN_4096 = 0x0c

PLEN_MNEMONICS = {
    DWT_PLEN_64: 64, DWT_PLEN_128: 128, DWT_PLEN_256: 256,
    DWT_PLEN_512: 512, DWT_PLEN_1024: 1024, DWT_PLEN_1536: 1536,
    DWT_PLEN_2048: 2048, DWT_PLEN_4096: 4096}

PRF_MNEMONICS = {
    DWT_PRF_16M: 16, DWT_PRF_64M: 64}

DATA_RATE_MNEMONICS  = {
    DWT_BR_110K: "110k", DWT_BR_850K: "850k", DWT_BR_6M8: "6800k"}

SHR_BASELEN = {
    DWT_PLEN_64: 64,
    DWT_PLEN_128: 128,
    DWT_PLEN_256: 256,
    DWT_PLEN_512: 512,
    DWT_PLEN_1024:1024,
    DWT_PLEN_1536:1536,
    DWT_PLEN_2048:2048,
    DWT_PLEN_4096:4096
}

class DwtConfig:

    def __init__(self, prf, data_rate, preamble_len):
        if not prf in PRF_MNEMONICS:
            raise ValueError("Invalid prf given")
        if not data_rate in DATA_RATE_MNEMONICS:
            raise ValueError("Invalid data rate given")
        if not preamble_len in PLEN_MNEMONICS:
            raise ValueError("Invalid preamble length given")
        self.prf = prf
        self.data_rate = data_rate
        self.preamble_len = preamble_len

    @classmethod
    def from_settings(cls, prf, datarate, plen):
        reversed_prf_mnemonic   = {v:k for k,v in PRF_MNEMONICS.items()}
        reversed_drate_mnemonic = {v:k for k,v in DATA_RATE_MNEMONICS.items()}
        reversed_plen_mnemonic  = {v:k for k,v in PLEN_MNEMONICS.items()}
        prf_code, drate_code, plen_code = reversed_prf_mnemonic[prf],\
                reversed_drate_mnemonic[datarate],\
                reversed_plen_mnemonic[plen]
        return DwtConfig(prf_code, drate_code, plen_code)

    def get_params_mnemonics(self):
        return PRF_MNEMONICS[self.prf],\
            DATA_RATE_MNEMONICS[self.data_rate],\
            PLEN_MNEMONICS[self.preamble_len]

    def __str__(self):
        return "prf %d, datarate %s, preamble length %d" %\
                (PRF_MNEMONICS[self.prf], DATA_RATE_MNEMONICS[self.data_rate],\
                PLEN_MNEMONICS[self.preamble_len])

    def estimate_tx_time(self, framelength, only_rmarker=False):

        if framelength < 0 or framelength > 127:
            raise ValueError("Framelength out of range")

        tx_time      = 0
        sym_timing_ind = 0
        shr_len      = 0

        # Symbol timing LUT
        SYM_TIM_16MHZ = 0
        SYM_TIM_64MHZ = 9
        SYM_TIM_110K  = 0
        SYM_TIM_850K  = 3
        SYM_TIM_6M8   = 6
        SYM_TIM_SHR   = 0
        SYM_TIM_PHR   = 1
        SYM_TIM_DAT   = 2

        SYM_TIM_LUT = [
        # 16 Mhz PRF
        994, 8206, 8206,  # 0.11 Mbps
        994, 1026, 1026,  # 0.85 Mbps
        994, 1026, 129,   # 6.81 Mbps
        # 64 Mhz PRF
        1018, 8206, 8206, # 0.11 Mbps
        1018, 1026, 1026, # 0.85 Mbps
        1018, 1026, 129   # 6.81 Mbps
        ]

        # Find the PHR
        if self.prf == DWT_PRF_16M:
            sym_timing_ind = SYM_TIM_16MHZ
        elif self.prf == DWT_PRF_64M:
            sym_timing_ind = SYM_TIM_64MHZ
        else:
            raise ValueError("Invalid prf configuration")

        shr_len = SHR_BASELEN[self.preamble_len]

        # Find the datarate
        if self.data_rate == DWT_BR_110K:
            sym_timing_ind  += SYM_TIM_110K
            shr_len         += 64  # SFD 64 symbols
        elif self.data_rate == DWT_BR_850K:
            sym_timing_ind  += SYM_TIM_850K
            shr_len         += 8   # SFD 8 symbols
        elif self.data_rate == DWT_BR_6M8:
            sym_timing_ind  += SYM_TIM_6M8
            shr_len         += 8   # SFD 8 symbols
        else:
            raise ValueError("Invalid data rate configuration")

        # Add the SHR time
        tx_time   = shr_len * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_SHR ]

        # If not only RMARKER, calculate PHR and data
        if only_rmarker is False:

            # Add the PHR time (21 bits)
            tx_time  += 21 * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_PHR ]

            # Bytes to bits
            framelength *= 8

            # Add Reed-Solomon parity bits
            framelength += REED_SOLOM_BITS * round(( framelength + DATA_BLOCK_SIZE - 1 ) / DATA_BLOCK_SIZE)
            #framelength = int(framelength)

            # Add the DAT time
            tx_time += framelength * SYM_TIM_LUT[ sym_timing_ind + SYM_TIM_DAT ]

        # Return in nano seconds
        return tx_time


# instance

DEFAULT_CONFIG = DwtConfig(DWT_PRF_64M, DWT_BR_6M8, DWT_PLEN_128)



if __name__ == "__main__":
    rmonly = DEFAULT_CONFIG.estimate_tx_time(20, True)
    full   = DEFAULT_CONFIG.estimate_tx_time(20, False)
    print(f"Full frame duration: {full}, SHR: {rmonly}, PHR+PSDU: {full-rmonly}")
