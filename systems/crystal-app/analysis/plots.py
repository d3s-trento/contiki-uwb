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

dd = pd.read_csv('./reliability_data.csv')
dd = dd.groupby(['fs', 'orig']).sum().reset_index()
dd['perc'] = 100*(dd['rcvd']/dd['sent'])
dd['epochs'] = dd['sent']/dd['orig']

print(dd)

@pu.plot(args)
def plot():
    fig, ax= plt.subplots(1, 1)
    dd0 = dd[dd['fs'] == 0].sort_values('orig')
    dd1 = dd[dd['fs'] == 1].sort_values('orig')

    x = np.arange(0, len(dd0))

    ax.plot(x, dd0['perc'], label=r'Crystal')
    ax.plot(x, dd1['perc'], label=r'Crystal w/ \textsc{Flick}')


    ax.set_ylabel(r'Reliability [\%]')
    ax.yaxis.set_major_formatter(mtick.PercentFormatter(decimals=3))

    ax.set_xticks(x)
    ax.set_xticklabels(dd0['orig'])
    ax.set_xlabel('U')

    ax.legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2)

    return fig

plot()
