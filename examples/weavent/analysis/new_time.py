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
import filtering_utilities as fu

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.WeaventLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser()

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--guard', dest='guard', default=0, type=float, help='the rx_guard used')

parser.add_argument('--repetitions', dest='repetitions', default=False, action='store_true', help='Show each repetition as separate from the rest')
parser.add_argument('--slot_idx', dest='slot_idx', default=False, action='store_true', help='Set to use slot_idx as the x axis instead of the repetitions')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--xlabel', dest='xlabel', default=None, type=str, help='Force a certain xlabel')

pu.add_plot_arguments(parser)

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

args = parser.parse_args()

print(args)

data = dict()

for l, jobs in args.logs:
    for job in jobs:
        job: jr.WeaventLogId

        with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes, fp_guard=args.fpGuard) as a:
            dd = a.time_event_data

            dd = fu.apply_epoch_filter(args, dd)

            if args.synchFilter:
                complete_bootstrap = set(a.complete_bootstrap['epoch'])

                removed = set(dd['epoch'].unique()) - complete_bootstrap

                if len(removed) > 0:
                    print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, fu.pp_epochs(removed)))

                dd = dd[dd['epoch'].isin(complete_bootstrap)]

            if args.completeEpochFilter:
                completed_epochs = set(a.completed_epochs['epoch'])

                removed = set(dd['epoch'].unique()) - completed_epochs

                if len(removed) > 0:
                    print('Filtering due incomplete epochs ({}): {}'.format(job.id, fu.pp_epochs(removed)))

                dd = dd[dd['epoch'].isin(completed_epochs)]

            if args.filterFalsePositives:
                falsePositives = a.false_positives

                if len(falsePositives) > 0:
                    print('Removing false postives ({}):\n {}'.format(job.id, falsePositives))

                dd = dd.merge(falsePositives[['node_id', 'epoch', 'repetition_i']], how='outer', on=['node_id', 'epoch', 'repetition_i'], indicator=True)
                dd = dd[dd._merge == 'left_only']

            dd['diff'] *= 1000  # Change from ms to us

        print(dd.groupby(['node_id']).describe()['diff'])

        el = dd.groupby(['epoch', 'repetition_i'])['diff'].max().reset_index()
        el['l'] = l

        del dd

        data[job.id] = el

for label, jobs in args.logs:
    for job in jobs:
        print(f'{job} {label}:')
        print('  Epochs: {}'.format(data[job.id]['epoch'].nunique()))
        print('  Points: {}'.format(len(data[job.id])))
        print('  Worse 25:\n{}'.format(data[job.id].sort_values('diff', ascending=False).head(25)))
        print('  Best 25:\n{}'.format(data[job.id].sort_values('diff', ascending=True).head(25)))

if not args.repetitions:
    print(pd.concat(list(data.values())).groupby('l').describe()['diff'])
else:
    print(pd.concat(list(data.values())).groupby(['l', 'repetition_i']).describe()['diff'])

@pu.plot(args, attach_subplots=True)
def plot():
    fig, axes = plt.subplots(nrows=1, ncols=2 if args.repetitions else 1, sharey=True, gridspec_kw={'width_ratios': [2, 18]} if args.repetitions else None)

    if args.repetitions:
        ax2 = axes[1]
        if len(args.logs) > 1:
            raise Exception('--repetition should be used only with one log')

        w = [np.array(list(itertools.chain.from_iterable(data[job.id][data[job.id]['repetition_i'] == i]['diff'].values for job in jobs))) for i in range(0, 100) for l, jobs in args.logs]
        xticks = [y+1 for y in range(0, 100, 9)]

        if args.slot_idx:
            xlabelticks = [f'{9+5*(i-1)}' for i in xticks  for l, job in args.logs]
            ax2.set_xlabel('slot index')
        else:
            xlabelticks = [f'{i}' for i in xticks for l, job in args.logs]
            ax2.set_xlabel('repetition')

        ax2.boxplot(w, sym='x', flierprops=dict(markersize=1, alpha=0.7), whis=(0.01, 0.99))
        p = ax2.violinplot(w, showextrema=False)

        ax2.set_xticks(xticks)
        ax2.set_xticklabels(xlabelticks)

    ax = axes[0] if isinstance(axes, np.ndarray) else axes

    if len(args.logs) > 1:
        ax.set_xlabel(args.xlabel or 'modality')

    w = [np.array(list(itertools.chain.from_iterable(data[job.id]['diff'].values for job in jobs)))  for _, jobs in args.logs]
    xticks = [y+1 for y in range(len(args.logs))]
    xlabelticks = [l for l, job in args.logs]

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7), whis=(0.01, 0.99))
    p = ax.violinplot(w, showextrema=False)

    ax.set_xticks(xticks)
    ax.set_xticklabels(xlabelticks)

    ax.set_ylabel(r'Max latency per Flick slot [\si{\us}]')

    if not (args.xcm is not None and args.ycm is not None):
        ax.get_figure().suptitle('Duration of Flick flood')
        ax.annotate('sink 19 w/o nodes 21,22 (5-6 hops)',
                    xy=(5, 5), xycoords='figure pixels')

    return fig

plot()
