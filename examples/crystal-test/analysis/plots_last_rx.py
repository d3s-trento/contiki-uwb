#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

import itertools

import numpy as np

import argparse

import plot_utility as pu

parser = argparse.ArgumentParser(
        description=''
)

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

dd = pd.read_csv('./last_rx_data.csv')

dd['last_rx_mean'] *= dd['epochs']

dd = dd.groupby(['fs', 'orig']).sum().reset_index()

dd['last_rx_mean'] /= dd['epochs']

dd['last_rx_mean'] *= 20 * 1.0256410256
dd['last_rx_mean'] /= 1000

tmp = dd

tmp['last_rx_mean'] = tmp['last_rx_mean'].round(2)

print(tmp)

@pu.plot(args)
def plot():
    fig, ax= plt.subplots(1, 1)
    dd0 = dd[dd['fs'] == 0].sort_values('orig')
    dd1 = dd[dd['fs'] == 1].sort_values('orig')

    x = np.arange(0, len(dd0))

    ax.plot(x, dd0['last_rx_mean'], label=r'Mean last rx(w/o FS)')

    ax.plot(x, dd1['last_rx_mean'], label=r'Mean last rx(w/ FS)')

    ax.set_ylabel(r'Time [\si{\ms}]')

    ax.set_xticks(x)
    ax.set_xticklabels(dd0['orig'])
    ax.set_xlabel('U')

    ax.legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2)

    return fig

plot()
