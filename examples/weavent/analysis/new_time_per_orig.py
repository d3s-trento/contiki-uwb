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


def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.WeaventLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser()
parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--no-sameEpochs', dest='sameEpochs', default=True, action='store_false', help='filter by only common epochs')
parser.add_argument('--sameEpochs',    dest='sameEpochs', default=True, action='store_true',  help='filter by only common epochs')

parser.add_argument('--min-time', dest='minTime', default=None, type=float, help='min value to show in the graph')
parser.add_argument('--max-time', dest='maxTime', default=None, type=float, help='max value to show in the graph')

parser.add_argument('-m', '--min-epoch', dest='minEpoch', default=None, type=int, help='min epoch to show in the graph')
parser.add_argument('-M', '--max-epoch', dest='maxEpoch', default=None, type=int, help='max epoch to show in the graph')

parser.add_argument('--numNodes', dest='numNodes', default=33, type=int, help='Number of nodes')
parser.add_argument('--guard', dest='guard', default=0, type=float, help='the rx_guard used')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--no-plot', dest='plot', default=True, action='store_false', help='Do not show the plot')

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

args = parser.parse_args()

print(args)

data = dict()

for l, jobs in args.logs:
    for job in jobs:
        job: jr.WeaventLogId

        with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes, fp_guard=args.fpGuard) as a:
            dd = a.time_event_data
            tx_fs = a.tx_fs

            tfs = tx_fs.groupby(['epoch','repetition_i']).aggregate(lambda x: frozenset(x)).reset_index()

            if args.epochFilter:
                eMin = dd['epoch'].min() + 10
                eMax = dd['epoch'].max() - 10

                dd = c.filter_by_epoch(dd, eMin, eMax)

            if args.synchFilter:
                incompleteBoostraps = a.incomplete_bootstrap['epoch']

                if len(incompleteBoostraps) > 0:
                    print('Removing epochs with incomplete boostraps ({}):\n {}'.format(job.id, incompleteBoostraps))

                dd = dd[~dd['epoch'].isin(incompleteBoostraps)]

            if args.filterFalsePositives:
                falsePositives = a.false_positives

                if len(falsePositives) > 0:
                    print('Removing false postives ({}):\n {}'.format(job.id, falsePositives))

                dd = pd.concat([dd, falsePositives])
                dd.drop_duplicates(['node_id', 'epoch', 'repetition_i'], keep=False, inplace=True)

                dd = dd[['epoch', 'repetition_i', 'node_id', 'diff']]

            dd = dd.merge(tfs, on=['epoch', 'repetition_i'])
            dd.rename(columns={'node_id_x': 'node_id', 'node_id_y': 'originators'}, inplace=True)

        el = dd.groupby(['originators', 'epoch', 'repetition_i'])['diff'].max().reset_index()
        el['l'] = l

        del dd

        data[job.id] = el

if args.minEpoch is not None:
    MIN = args.minEpoch
else:
    MIN = 10

if args.maxEpoch is not None:
    MAX = args.maxEpoch
elif args.sameEpochs:
    MAX = min(data[job.id]['epoch'].max()-50 for _, jobs in args.logs for job in jobs)
else:
    MAX = -10

if args.epochFilter or args.sameEpochs:
    for l, jobs in args.logs:
        for job in jobs:
            data[job.id] = c.filter_by_epoch(data[job.id], MIN, MAX)

print(pd.concat(list(data.values()))
        .groupby('originators')
        .describe()['diff']
        .reset_index()
        .sort_values('originators', key=lambda x: sorted(list(x))))

if args.plot:
    fig, axes = plt.subplots(nrows=1, ncols=2 , sharey=True, gridspec_kw={'width_ratios': [1, 6]})

    ax2 = axes[1]
    if len(args.logs) > 1:
        raise Exception('--repetition should be used only with one log')

    job_id = args.logs[0][1][0].id

    dd = data[job_id]

    xticks = sorted(dd['originators'].unique(), key=lambda x: sorted(list(x)))
    w = [np.array(dd[dd['originators'] == o]['diff'].values) for o in xticks]

    xlabelticks = [f'{set(i)}' for i in xticks]
    ax2.set_xlabel('originators')

    ax2.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax2.violinplot(w, showextrema=False)

    ax2.set_xticks([i+1 for i in range(len(xticks))])
    ax2.set_xticklabels(xlabelticks)

    ax2.annotate('sink 19 w/o nodes 21,22 (5-6 hops)',
                xy=(5, 5), xycoords='figure pixels')

    ax = axes[0] if isinstance(axes, np.ndarray) else axes

    if len(args.logs) > 1:
        ax.set_xlabel('modality')

    w = [np.array(list(itertools.chain.from_iterable(data[job.id]['diff'].values for job in jobs)))  for _, jobs in args.logs]
    xticks = [y+1 for y in range(len(args.logs))]
    xlabelticks = [l for l, job in args.logs]

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax.violinplot(w, showextrema=False)

    ax.set_xticks(xticks)
    ax.set_xticklabels(xlabelticks)

    ax.annotate('sink 19 w/o nodes 21,22 (5-6 hops)',
                xy=(5, 5), xycoords='figure pixels')

    ax.set_ylabel('max latency per epoch (ms)')
    ax.get_figure().suptitle('Duration of fast propagating flood')

    ax.set_ylim([args.minTime, args.maxTime])

    plt.tight_layout()
    plt.subplots_adjust(wspace=0, hspace=0)
    plt.show()

