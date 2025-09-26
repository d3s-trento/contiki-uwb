#!/usr/bin/env python3

import argparse

import pandas as pd
import matplotlib.pyplot as plt

import common as c

import weavent_jobref as jr

import plot_utility as pu
import filtering_utilities as fu

parser = argparse.ArgumentParser()

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--no-sameEpochs', dest='sameEpochs', default=True, action='store_false', help='filter by only common epochs')
parser.add_argument('--sameEpochs',    dest='sameEpochs', default=True, action='store_true',  help='filter by only common epochs')

parser.add_argument('--guard', dest='guard', default=0, type=float, help='the rx_guard used')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

pu.add_plot_arguments(parser)

parser.add_argument('log', type=int, help='(Single) log to use')

args = parser.parse_args()

print(args)

data = dict()

job: jr.WeaventLogId = jr.WeaventLogId(args.log)

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

print(dd.groupby('node_id').describe()['diff'])

@pu.plot(args, attach_subplots=True)
def plot():
    if args.xcm is not None and args.ycm is not None:
        plt.rcParams.update({
            'xtick.labelsize': 6,
            'ytick.labelsize': 6,
            'axes.labelsize':  7,
        })

    fig, (ax, ax2) = plt.subplots(nrows=1, ncols=2 , sharey=True, gridspec_kw={'width_ratios': [2, 18]}, constrained_layout=(args.xcm is not None and args.ycm is not None))

    xticks = sorted(dd['node_id'].unique())
    w = [dd[dd['node_id'] == nid]['diff'] for nid in xticks]

    xlabelticks = [nid for nid in xticks]

    ax2.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax2.violinplot(w, showextrema=False)

    del w

    ax2.set_xlabel('node_id')
    ax2.set_xticks(list(range(1, len(xticks)+1)))
    ax2.set_xticklabels(xlabelticks)

    ax.boxplot(dd['diff'], sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax.violinplot(dd['diff'], showextrema=False)

    ax.set_xticks([])
    ax.set_ylabel(r'max latency per epoch [\si{\us}]')

    if not (args.xcm is not None and args.ycm is not None):
        ax.get_figure().suptitle('Duration of fast propagating flood')

    return fig

plot()
