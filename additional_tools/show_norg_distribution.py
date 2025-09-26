#!/usr/bin/env python3

import argparse

import pandas as pd
from matplotlib import pyplot as plt
import matplotlib.ticker
import scipy.interpolate
import numpy as np

import plot_utility as pu

def pair_name_path(a: str):
    name, path = a.split(';', maxsplit=1)
    return name, path

parser = argparse.ArgumentParser(
        description='Show how the energy consumption would be using a certain energy distribution wrt the number of originators and a certain distribution of originators'
)

parser.add_argument('--zoom', dest='zoom', type=float, nargs='+', help='Zoom with xmin and ymin')

parser.add_argument('norg', type=pair_name_path, nargs='+', help='The csv to use for the number of originators distribution')

pu.add_plot_arguments(parser)

args = parser.parse_args()

if args.zoom is not None and len(args.zoom) != 2:
    raise Exception('Zoom arguments should be 2 (xmin , ymin)')

print(args)

data = []

for norg_name, norg_path in args.norg:
    norg = pd.read_csv(norg_path)

    norg.sort_values('updates', inplace=True)

    norg[' epochs'] /= norg[' epochs'].sum()

    print(norg)

    data.append((norg_name, norg))

offset = 0.1
divd = (1 - 2*offset)/len(data)

def pp(ax):
    for i, hatch, (norg_name, norg_data) in zip(range(len(data)), ['/', '\\', 'o', 'O', '.', '*', '-', 'x', '+', '|'], data):
        ax.bar(norg_data['updates'] + (i-int(len(data)/2))*divd, norg_data[' epochs'], label=norg_name, width=divd)

@pu.plot(args)
def plot():

    fig, ax = plt.subplots(1, 1)

    pp(ax)

    ax.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter(1))

    xticks = list(range(max(norg_data['updates'].max() for norg_name, norg_data in data) + 1))

    ax.set_xticks(xticks)

    ax.set_ylabel('Percentage of the epochs')
    ax.set_xlabel('Number of updates per epoch')
    ax.axvline(0.5, color='black', lw=plt.rcParams['axes.linewidth'])

    ax.legend()

    if args.zoom is not None:
        axins = ax.inset_axes([0.30, 0.15, 0.67, 0.65])
        pp(axins)

        axins.set_xticks(xticks)
        axins.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter(1))

        axins.set_xlim(args.zoom[0] - int(len(data)/2)*divd - divd, ax.get_xlim()[1])
        axins.set_ylim(0, args.zoom[1])

        axins.tick_params(left=True, right=False, labelleft=False, labelbottom=True, bottom=True)

        axins.margins(0)

        ax.indicate_inset_zoom(axins, edgecolor='black')

    return fig

plot()
