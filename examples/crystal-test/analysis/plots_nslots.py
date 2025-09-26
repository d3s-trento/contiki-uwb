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

dd = pd.read_csv('./nslots_data.csv')

dd['mean_min'] *= dd['epochs']
dd['mean'] *= dd['epochs']
dd['mean_max'] *= dd['epochs']

dd = dd[['fs', 'orig', 'epochs', 'mean_min', 'mean', 'mean_max']].groupby(['fs', 'orig']).sum().reset_index()

dd['mean_min'] /= dd['epochs']
dd['mean'] /= dd['epochs']
dd['mean_max'] /= dd['epochs']

dd['mean_min'] *= 20 * 1.0256410256
dd['mean'] *= 20 * 1.0256410256
dd['mean_max'] *= 20 * 1.0256410256

dd['mean_min'] /= 1000
dd['mean'] /= 1000
dd['mean_max'] /= 1000

tmp = dd

tmp['mean_min'] = tmp['mean_min'].round(2)
tmp['mean'] = tmp['mean'].round(2)
tmp['mean_max'] = tmp['mean_max'].round(2)

print(tmp)

@pu.plot(args)
def plot():
    fig, ax= plt.subplots(1, 1)
    dd0 = dd[dd['fs'] == 0].sort_values('orig')
    dd1 = dd[dd['fs'] == 1].sort_values('orig')

    x = np.arange(0, len(dd0))

    ax.plot(x, dd0['mean_min'], label=r'Mean min termination (w/o FS)')
    ax.plot(x, dd0['mean'], label=r'Mean termination (w/o FS)')
    ax.plot(x, dd0['mean_max'], label=r'Mean max termination (w/o FS)')

    ax.plot(x, dd1['mean_min'], label=r'Mean min termination (w/ FS)')
    ax.plot(x, dd1['mean'], label=r'Mean termination (w/ FS)')
    ax.plot(x, dd1['mean_max'], label=r'Mean max termination (w/ FS)')

    ax.set_ylabel(r'Time [\si{\ms}]')

    ax.set_xticks(x)
    ax.set_xticklabels(dd0['orig'])
    ax.set_xlabel('U')

    ax.legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2)

    return fig

plot()
