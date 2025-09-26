#!/usr/bin/env python3

import argparse

import pandas as pd
from matplotlib import pyplot as plt
import scipy.interpolate
import numpy as np

import plot_utility as pu

def pair_name_path(a: str):
    return a.split(';', maxsplit=1)

parser = argparse.ArgumentParser(
        description='Show how the energy consumption would be using a certain energy distribution wrt the number of originators and a certain distribution of originators'
)

parser.add_argument('energy', type=pair_name_path, nargs='+', help='The csvs to use for the energy distribution')
parser.add_argument('--norg', type=pair_name_path, nargs='+', action='store', help='The csv to use for the number of originators distribution')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

finalData = pd.DataFrame(columns=['energy_distribution', 'norg_distribution', 'mean_energy'])

for energy_name, energy_path in args.energy:
    energy = pd.read_csv(energy_path)
    energy.sort_values('norg', inplace=True)

    print(energy)

    w = scipy.interpolate.interp1d(energy['norg'], energy['mean'], kind='next')

    for norg_name, norg_path in args.norg:
        norg = pd.read_csv(norg_path)

        energy = pd.DataFrame([(i, w(i)) for i in range(min(norg['updates'].min(),energy['norg'].min()), max(norg['updates'].max(), energy['norg'].max())+1)], columns=['norg', 'mean_energy'])

        finalEnergy = pd.merge(energy, norg,  left_on='norg', right_on='updates')
        finalEnergy['energy_per_epoch'] = finalEnergy['mean_energy']
        finalEnergy['total_energy'] = finalEnergy['mean_energy'] * finalEnergy[' epochs']
        finalEnergy['mean_energy'] = finalEnergy['total_energy'] / finalEnergy[' epochs'].sum()

        print(finalEnergy)

        finalData.loc[len(finalData)] = [energy_name, norg_name, finalEnergy['mean_energy'].sum()]

print(finalData.sort_values(['norg_distribution', 'energy_distribution']))

@pu.plot(args)
def plot():
    finalData.set_index('norg_distribution', inplace=True)
    axes = finalData.groupby('energy_distribution')['mean_energy'].plot(marker='x', legend=True, ylabel=r'Mean energy consumption per epoch [\si{\uJ}]')

    s = set([k.get_figure() for k in axes])

    if len(s) != 1:
        raise Exception('There is more than one figure. This is not supported')

    fig = list(s)[0].get_figure()

    fig.axes[0].set_xticks(np.arange(0, len(args.norg)))
    fig.axes[0].set_xlabel('Dataset')

    fig.axes[0].legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2)

    return fig

plot()
