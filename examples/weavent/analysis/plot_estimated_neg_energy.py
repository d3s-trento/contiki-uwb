#!/usr/bin/env python3

import plot_utility as pu

import argparse

import itertools

import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import os

import numpy as np

import common as c

import weavent_jobref as jr

import plot_utility as pu
import filtering_utilities as fu

import sys

print(sys.argv)


def label_id_pair(a: str):
    label, rx_time_uus, rx_time_us, median_min, median, median_max = a.split(';', 5)

    v = (int(label), float(rx_time_uus), float(rx_time_us), float(median_min), float(median), float(median_max))

    return v

parser = argparse.ArgumentParser()

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('-phl', dest='phl', default=True, action='store_true', help='Show per hop latency')
parser.add_argument('-nphl', dest='phl', default=True, action='store_false', help='Do not show per-hop latency')

parser.add_argument('-e', dest='e', default=True, action='store_true', help='Show energy plot')
parser.add_argument('-ne', dest='e', default=True, action='store_false', help='Do not show energy plot')

pu.add_plot_arguments(parser)

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

args = parser.parse_args()

print(args)

data = []

for l, rx_time_uus, rx_time_us, median_min, median, median_max in args.logs:
    e_tx_preamble = 83.0
    e_tx_data = 52.0
    e_rx_hunting = 113.0
    e_rx_preamble = 113.0
    e_rx_data = 118.0
    e_idle = 18.0
    ref_voltage = 3.3  # V

    e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data = tuple(map(lambda curr_mA: curr_mA / 1e3 * ref_voltage,\
                                                                                 (e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data)))

    t_hunting = (rx_time_us*1000)*(16*254/(16*254 + l*256))
    t_idle = (rx_time_us*1000) - t_hunting

    e_idle    = t_idle * e_idle
    e_hunting = t_hunting * e_rx_hunting

    e_total = e_idle + e_hunting
    e_total /= 1000

    data.append((l, e_total, rx_time_uus, rx_time_us, median_min, median, median_max))

print(data)

@pu.plot(args, attach_subplots=False)
def plot():
    fig, axs = plt.subplots(nrows=int(args.phl) + int(args.e), ncols=1, sharex=int(args.phl) + int(args.e) > 1)

    axs = np.array([axs]) if not isinstance(axs, np.ndarray) else axs

    dd = pd.DataFrame({
        'sniff': np.array([l for l, _, _, _, _, _, _ in data]),
        'median': np.array([median for _, _, _, _, _, median, _ in data]),
        'median_min': np.array([median_min for _, _, _, _, _, _, median_min in data]),
        'median_max': np.array([median_max for _, _, _, _, median_max, _, _ in data]),
        'energy': np.array([e_total for l, e_total, _, _, _, _, _ in data])
    })

    print(dd)

    if args.phl:
        ax = axs[0]

        #ax.errorbar(dd['sniff'], dd['median'], yerr=(dd['median'] - dd['median_min'], dd['median_max'] - dd['median'] ), label='Per-hop latency', color='tab:orange', marker='.')
        ax.plot(dd['sniff'], dd['median'], marker='.')
        ax.set_ylabel(r'Latency [\si{\us}]')

        if not (int(args.phl) + int(args.e) > 1):
            ax.set_xlabel(r'Sniff $T_{idle}$')
            ax.set_xticks(np.arange(dd['sniff'].min(), dd['sniff'].max()+1, 8))
        else:
            ax.xaxis.set_visible(False)

        ax.grid(True, axis='y')

    if args.e:
        ax = axs[int(args.phl)]
        ax.plot(dd['sniff'], dd['energy'] , marker='.')
        ax.set_ylabel(r'Energy [\si{\uJ}]')
        ax.set_xlabel(r'Sniff $T_{idle}$')

        ax.set_xticks(np.arange(dd['sniff'].min(), dd['sniff'].max()+1, 8))
        ax.grid(True, axis='y')

    return fig

plot()
