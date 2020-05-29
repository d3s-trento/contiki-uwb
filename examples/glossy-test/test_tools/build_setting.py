#!/usr/bin/python3
import re
from collections import OrderedDict

import pandas as pd

from txtime import PLEN_MNEMONICS, PRF_MNEMONICS, DATA_RATE_MNEMONICS

# -----------------------------------------------------------------------------
# SETTINGS
# -----------------------------------------------------------------------------
SIM_SETTINGS = "settings.csv"
# fields
SIM_NTX = "ntx"
SIM_VERSION = "version"
SIM_SLOT_EST= "slot_estimation"
SIM_INITIATOR = "initiator"
SIM_SMART_TX = "smartx"
SIM_TXPOWER = "txpower"
SIM_FRAMESIZE = "frame_size"
SIM_SLOT_DURATION_MS = "slot_ms"
SIM_PERIOD_DURATION_MS = "period_ms"
SIM_GUARD_TIME_MS = "guard_ms"
SIM_CHANNEL = "channel"
SIM_PRF     = "prf"
SIM_PRLEN   = "prlen"
SIM_DATA_RATE = "datarate"
SIM_SFD_MODE = "sfd_mode"
# temporary field
SIM_PAYLOAD_LEN = "payload_len"

# HEADERS
SETTINGS_HEADER = [SIM_CHANNEL, SIM_PRF, SIM_PRLEN, SIM_DATA_RATE, SIM_SFD_MODE,\
        SIM_SMART_TX, SIM_TXPOWER, SIM_VERSION, SIM_SLOT_EST, SIM_NTX, SIM_FRAMESIZE, SIM_INITIATOR]
SETTINGS_ABBREV = ["ch", "prf", "prlen", "drate", "sfdmode", "smarttx", "txpower", "ver", "est", "ntx", "frame", "init"]

SETTINGS_FULL = [
        SIM_PERIOD_DURATION_MS,  SIM_SLOT_DURATION_MS, SIM_GUARD_TIME_MS,\
        SIM_CHANNEL, SIM_PRF, SIM_PRLEN, SIM_DATA_RATE, SIM_SFD_MODE,\
        SIM_SMART_TX, SIM_TXPOWER,\
        SIM_VERSION, SIM_SLOT_EST, SIM_FRAMESIZE, SIM_NTX, SIM_INITIATOR]


FILTER_RULES = {
        "(?i)dw1000_channel:\s+(\d+)": SIM_CHANNEL,\
        "(?i)dw1000_prf:\s+(\d+)": SIM_PRF,\
        "(?i)dw1000_data_rate:\s+(\d+)": SIM_DATA_RATE,\
        "(?i)dw1000_plen:\s+(\w+)": SIM_PRLEN,\
        "(?i)dw1000_sfd_mode:\s+(\d+)": SIM_SFD_MODE,\
        "(?i)dw1000_smart_tx_power_6m8:\s+(\d+)": SIM_SMART_TX,\
        "(?i)dw1000_conf_tx_power:\s+(\w+)": SIM_TXPOWER,\
        "(?i)glossy_version:\s+(\w+)": SIM_VERSION,\
        "(?i)initiator_id:\s+(\d+)": SIM_INITIATOR,\
        "(?i)payload_data_len:\s+(\d+)": SIM_PAYLOAD_LEN,\
        "(?i)glossy_n_tx:\s+(\d+)": SIM_NTX,\
        "(?i)glossy_dynamic_slot_estimate:\s+(\d+)": SIM_SLOT_EST,\
        "(?i)glossy_period:\s+(.*)": SIM_PERIOD_DURATION_MS,\
        "(?i)glossy_t_slot:\s+(.*)": SIM_SLOT_DURATION_MS,\
        "(?i)glossy_t_guard:\s+(.*)": SIM_GUARD_TIME_MS\
}

def match_filter(log, rules):
    """
    Return None if there is no rule in rules matching
    the log.

    Return the log if it matches a rule or None.
    """
    for rule in rules:
        match = re.match(rule, log)
        if match:
            return rules[rule], match
    return None, None

def get_pkt_size(payload):
    # header (6B) + seqno (4B) + payload + <byte-alignment due to payload offset> + crc(2B)
    # payload is 4byte aligned
    return 6 + payload + 4 + 2

def parse_build_setting_lines(lines):
    RTIMER_SYMBOLIC = r"rtimer_second"
    RTIMER_SECOND = 32768
    PRECISION  = 1000 # ms
    # assume millisecond granularity
    # if this is not sufficient (IT SHOULD BE!) throw an error an inform
    # the user that this code should be changed to the new precision
    # by changing the value of RTIMER_VALUE

    settings = {}
    for line in lines:
        rule, match = match_filter(line, FILTER_RULES)
        if rule == SIM_CHANNEL:
            settings[SIM_CHANNEL] = int(match.group(1))
        elif rule == SIM_PRF:
            prf_code = int(match.group(1))
            settings[SIM_PRF] = PRF_MNEMONICS[prf_code]
        elif rule == SIM_DATA_RATE:
            dr_code = int(match.group(1))
            settings[SIM_DATA_RATE] = DATA_RATE_MNEMONICS[dr_code]
        elif rule == SIM_PRLEN:
            pl_code = int(match.group(1), 16)
            settings[SIM_PRLEN] = PLEN_MNEMONICS[pl_code]

        elif rule == SIM_SFD_MODE:
            settings[SIM_SFD_MODE] = int(match.group(1))

        elif rule == SIM_SMART_TX:
            settings[SIM_SMART_TX] = int(match.group(1))

        elif rule == SIM_TXPOWER:
            settings[SIM_TXPOWER] = match.group(1)

        elif rule == SIM_INITIATOR:
            settings[SIM_INITIATOR] = int(match.group(1))

        elif rule == SIM_PAYLOAD_LEN:
            settings[SIM_FRAMESIZE] = get_pkt_size(int(match.group(1)))

        elif rule == SIM_SLOT_EST:
            est = int(match.group(1))
            if est == 0:
                settings[SIM_SLOT_EST] = "static"
            elif est == 1:
                settings[SIM_SLOT_EST] = "dynamic"
            else:
                raise ValueError("Unknown slot estimation")

        elif rule == SIM_VERSION:
            version = match.group(1).lower()
            if version == "glossy_tx_only_version":
                settings[SIM_VERSION] = "txo"
            elif version == "glossy_standard_version":
                settings[SIM_VERSION] = "std"
            else:
                raise ValueError("Unknown Glossy version found")

        elif rule == SIM_NTX:
            settings[SIM_NTX] = int(match.group(1))

        elif rule == SIM_PERIOD_DURATION_MS:
            duration = match.group(1)
            duration = int(eval(duration) / RTIMER_SECOND * PRECISION)
            settings[SIM_PERIOD_DURATION_MS] = duration

        elif rule == SIM_SLOT_DURATION_MS:
            duration = match.group(1)
            duration = int(eval(duration) / RTIMER_SECOND * PRECISION)
            settings[SIM_SLOT_DURATION_MS] = duration

        elif rule == SIM_GUARD_TIME_MS:
            duration = match.group(1)
            duration = int(eval(duration) / RTIMER_SECOND * PRECISION)
            settings[SIM_GUARD_TIME_MS] = duration

    return settings

def parse_build_setting(filesettings):
    with open(filesettings, "r") as fh:
        return parse_build_setting_lines(fh)

def get_settings_row(settings):
    values = [settings[h] for h in SETTINGS_HEADER]
    return values

def get_radio_fixed_settings(settings):
    return settings[SIM_CHANNEL], settings[SIM_PRF], settings[SIM_PRLEN], settings[SIM_DATA_RATE], settings[SIM_SFD_MODE]

def get_sim_name(settings):
    values = [settings[v] for v in SETTINGS_HEADER]
    values = ["%s%s" % (k, str(v)) for k,v in zip(SETTINGS_HEADER, values)]
    return str.join("_", values)

def get_sim_name_abbrev(settings):
    values = [str(settings[v]).lower() for v in SETTINGS_HEADER]
    values = ["%s%s" % (k, str(v)) for k,v in zip(SETTINGS_ABBREV, values)]
    return str.join("_", values)


def get_settings_summary(settings):
    summary=OrderedDict([(k, settings[k]) for k in SETTINGS_FULL])
    return summary

def get_conf_settings(csv_settings_file):
    """Return an ordered dictionary with required fields and values
    corresponding to the settings configuration retrieved from
    the given csv file.
    """
    a_row = {k:v[0] for k,v in pd.read_csv(csv_settings_file).to_dict(orient="list").items()}
    settings_odict = OrderedDict((k,a_row[k]) for k in SETTINGS_HEADER)
    return settings_odict


if __name__ == "__main__":
    pass
