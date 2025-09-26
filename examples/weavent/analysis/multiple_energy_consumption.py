#!/usr/bin/env python3
import pandas as pd
from matplotlib import pyplot as plt
import argparse

import plot_utility as pu

parser = argparse.ArgumentParser(
        description='Show the energy consumption difference between the different sniff_off and #originators'
)

pu.add_plot_arguments(parser)

parser.add_argument('csv', type=str,help='the csv to use')

args = parser.parse_args()

print(args)

dd = pd.read_csv(args.csv)

@pu.plot(args, attach_subplots=True)
def plot():
    fig, axes = plt.subplots(3, 1)

    for (norg, data), ax in zip(dd.groupby('norg'), axes):
        ax.plot(data['so'], (data['mean'])/100, label=f'{norg} originators', marker='x')

        ax.set_title(f'{norg} originators')

        ax.set_xticks(data['so'].unique())

        ax.set_ylabel(r'Energy consumption [\si{\uJ}]')
        ax.set_xlabel(r'Sniff off time [\si{\us}]')

    return fig

plot()
