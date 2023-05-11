#!/usr/bin/env python3

import sys
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

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.WeaventLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser()
parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--numNodes', dest='numNodes', default=33, type=int, help='Number of nodes')
parser.add_argument('--guard', dest='guard', default=0, type=float, help='the rx_guard used')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--xlabel', dest='xlabel', default=None, type=str, help='Force a certain xlabel')

pu.add_plot_arguments(parser)

parser.add_argument('logs', type=int, nargs='+', help='List of logs')

args = parser.parse_args()

print(args)

data = []

for log in args.logs:
    job: jr.WeaventLogId = jr.WeaventLogId(log)

    with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes) as a:
        tfs = a.tx_fs

        if args.epochFilter:
            eMin = tfs['epoch'].min() + 10
            eMax = tfs['epoch'].max() - 10

            tfs = c.filter_by_epoch(tfs, eMin, eMax)

        if args.synchFilter:
            incompleteBoostraps = a.incomplete_bootstrap['epoch']

            if len(incompleteBoostraps) > 0:
                print('Removing epochs with incomplete boostraps ({}):\n {}'.format(job.id, incompleteBoostraps))

            tfs = tfs[~tfs['epoch'].isin(incompleteBoostraps)]

        tfs['id'] = job.id

        data.append(tfs)

data = pd.concat(data)

data = data.groupby(['id', 'epoch', 'repetition_i']).size().reset_index().rename(columns={0: 'norg'})
data = data.groupby('norg').size().reset_index().rename(columns={0: 'instances'})
print(data)

@pu.plot(args)
def plot():
    if args.xcm is not None and args.ycm is not None:
        plt.rcParams.update({
            'xtick.labelsize': 6,
            'ytick.labelsize': 6,
            'axes.labelsize':  7,
        })

    import matplotlib.ticker

    fig, ax = plt.subplots(nrows=1, ncols=1)

    ax.bar(data['norg'], data['instances'], width=1)

    ax.set_xticks(np.arange(data['norg'].min(), data['norg'].max()+1))
    ax.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter(data['instances'].sum()))

    ax.set_ylabel(r'Percentage of epochs [\%]')
    ax.set_xlabel(r'Number of originators')

    ax.set_ylim(0, data['instances'].sum())

    # ax.set_xticks(list(range(1, 35)))

    return fig

plot()
