#!/usr/bin/env python3

import glossy_jobref as jr

import numpy as np
import pandas as pd

import matplotlib.pyplot as plt
from matplotlib.ticker import EngFormatter

import plot_utility as pu
import filtering_utilities as fu

import argparse
import itertools

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.GlossyLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser()

fu.add_epoch_filter(parser, True, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--xlabel', dest='xlabel', default=None, type=str, help='Force a certain xlabel')

parser.add_argument('--rel', dest='rel', default=False, action='store_true', help='Put results in relation to the base reliability')
parser.add_argument('--baseRel', dest='baseRel', default=1, type=float, help='Base reliability')

pu.add_plot_arguments(parser)

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

args = parser.parse_args()

print(args)

P = [0.5, 0.99, 0.999, 0.9999, 0.99999]

data = dict()

for l, jobs in args.logs:
    dds = []
    for job in jobs:
        job: jr.GlossyLogId

        with job.access(args.force, num_nodes=args.numNodes) as a:
            dd = a.latency

            dd = fu.apply_epoch_filter(args, dd)

            if args.synchFilter:
                completeBoostraps = set(a.complete_bootstrap['epoch'])

                removed = set(dd['epoch'].unique()) - completeBoostraps

                if len(removed) > 0:
                    print('Removing epochs with incomplete boostraps ({}):\n {}'.format(job.id, removed))

                dd = dd[dd['epoch'].isin(completeBoostraps)]

            if args.completeEpochFilter:
                completeEpoch = set(a.completed_epochs['epoch'])

                removed = set(dd['epoch'].unique()) - completeEpoch

                if len(removed) > 0:
                    print('Removing epochs with incomplete completions ({}):\n {}'.format(job.id, removed))

                dd = dd[dd['epoch'].isin(completeEpoch)]

            dd['duration'] /= 1000

        #el = dd.groupby(['epoch', 'repetition_i'])['duration'].max().reset_index()
        el = dd
        el['l'] = l
        el['job_id'] = job.id

        dds.append(dd)

        del dd

    data[l] = pd.concat(dds)

for label, jobs in args.logs:
    print(f'{label}:')
    print('  Epochs: {}'.format(len(data[label]['epoch'].unique())))
    print('  Points: {}'.format(len(data[label])))
    print(data[label])
    print('  By node_id:\n{}'.format(data[label].groupby(['node_id']).describe()['duration']))
    print('  Worse 25:\n{}'.format(data[label].sort_values('duration', ascending=False).head(25)))

    d = data[label].groupby(['job_id', 'epoch', 'repetition_i']).max()['duration']
    #d = data[label]['duration']

    print('  Latency percentiles:')
    for p in P:
        if not args.rel:
            print('    {}'.format((p, np.percentile(d, 100 * p))))
        elif args.baseRel > p:
            print('    {}'.format((p, np.percentile(d, 100 * (p/args.baseRel)))))

print(pd.concat(list(data.values())).groupby('l').describe()['duration'])

@pu.plot(args, attach_subplots=True)
def plot():
    fig, ax = plt.subplots(nrows=1, ncols=1, sharey=True)

    if len(args.logs) > 1:
        ax.set_xlabel(args.xlabel or 'modality')

    w = [
            data[label]['duration']/1e6 # np.array(list(itertools.chain.from_iterable(data[job.id]['duration'].values for job in jobs)))
            for label, jobs in args.logs
    ]
    xticks = [y+1 for y in range(len(args.logs))]
    xlabelticks = [l for l, job in args.logs]

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    ax.violinplot(w, showextrema=False)

    ax.set_xticks(xticks)
    ax.set_xticklabels(xlabelticks)

    ax.set_ylabel(r'Max latency per Glossy slot')
    ax.yaxis.set_major_formatter(EngFormatter(unit=r's'))

    if not (args.xcm is not None and args.ycm is not None):
        ax.get_figure().suptitle('Duration of Glossy flood')

    return fig


plot()

