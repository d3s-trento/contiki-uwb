#!/usr/bin/env python3
import argparse

import os
import stat

import pandas as pd
import subprocess

import statetime
import jobref

parser = argparse.ArgumentParser(description='Compute energy consumption from statetime traces')

parser.add_argument('job',
                    type=lambda x: jobref.from_job_log(x, 'statetime_traces.csv'),
                    help='The jobid/file to use')

parser.add_argument('-m', '--minEpoch',
                    dest='minEpoch',
                    required=False,
                    default=0,
                    type=int,
                    help='Filter out epochs before this (excluded)')
parser.add_argument('-M', '--maxEpoch',
                    dest='maxEpoch',
                    required=False,
                    default=None,
                    type=int,
                    help='Filter out epochs after this (excluded)')
parser.add_argument('-o', '--savedir',
                    dest='savedir',
                    required=False,
                    default='.',
                    type=str,
                    help='Folder into which put the results')
parser.add_argument('-f', '--force',
                    dest='force',
                    default=False,
                    type=bool,
                    help='Force the calculation instead of using cache')

args = parser.parse_args()

raise Exception('TODO: Is this even usefult now that I have statetime.py')

print(args)

if not os.path.isdir(args.savedir):
    raise Exception("Savedir directory is not a directory")

traces_pd = statetime.get(args.job, force=args.force)

MAX_T = 4e6

if args.maxEpoch is None:
    args.maxEpoch = traces_pd['epoch'].max()

# remove epoch where an underflow occurred
mask = (traces_pd.idle > MAX_T) | (traces_pd.rx_hunting > MAX_T) |\
    (traces_pd.tx_preamble > MAX_T) | (traces_pd.tx_data > MAX_T) |\
    (traces_pd.rx_preamble > MAX_T) | (traces_pd.rx_data > MAX_T)
epochs_to_remove = traces_pd[mask].epoch.unique()

if len(epochs_to_remove) > 0:
    print("Found statetime underflows, removing the following epochs from energy computation:\n{}\nlen: {}\nepochs: {}".format(traces_pd[mask].head(), len(traces_pd[mask]), traces_pd[mask]['epoch'].unique()))

traces_pd = traces_pd[~traces_pd.epoch.isin(epochs_to_remove)]

# Configuration Considered: Ch 2, plen 128, 64Mhz PRF, 6.8Mbps Datarate
# Setting N of page 30 DW1000 Datasheet ver 2.17.
# Average consumption coefficients considered.
e_tx_preamble = 83.0
e_tx_data = 52.0
e_rx_hunting = 113.0
e_rx_preamble = 113.0
e_rx_data = 118.0
e_idle = 18.0
ref_voltage = 3.3 #V

e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data = tuple(map(lambda curr_mA: curr_mA / 1e3 * ref_voltage,\
                                                                             (e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data)))

traces_pd["idle"] =         traces_pd["idle"]        * e_idle
traces_pd["tx_preamble"] =  traces_pd["tx_preamble"] * e_tx_preamble
traces_pd["tx_data"] =      traces_pd["tx_data"]     * e_tx_data
traces_pd["rx_hunting"] =   traces_pd["rx_hunting"]  * e_rx_hunting
traces_pd["rx_preamble"] =  traces_pd["rx_preamble"] * e_rx_preamble
traces_pd["rx_data"] =      traces_pd["rx_data"]     * e_rx_data

traces_pd["e_total"] = traces_pd["idle"] +\
    traces_pd["tx_preamble"] + traces_pd["tx_data"] +\
    traces_pd["rx_hunting"] + traces_pd["rx_preamble"] + traces_pd["rx_data"]

traces_pd[(traces_pd['epoch'] >= args.minEpoch) & (traces_pd['epoch'] <= args.maxEpoch)].to_csv(os.path.join(args.savedir, "energy.csv"), index=False)

traces_pd.drop("epoch", axis=1, inplace=True)

statetime = traces_pd[["node_id", "e_total"]].groupby("node_id").agg(["mean", "std"]).reset_index()
statetime.columns = ["node_id", "e_total_mean", "e_total_sd"]
statetime.to_csv(os.path.join(args.savedir, "pernode_energy.csv"), index=False)
