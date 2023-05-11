#!/usr/bin/env python3

import argparse

from matplotlib.ticker import FuncFormatter
import matplotlib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import common as c

import weavent_jobref as jr

import plot_utility as pu

import filtering_utilities as fu

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.WeaventLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser(
        description='Show how the latency is distributed'
)

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--guard', dest='guard', default=0, type=float, help='the rx_guard used')

parser.add_argument('--no-cumulative', dest='cumulative', default=False, action='store_false', help='Show the density or the cumulative distribution')
parser.add_argument('--cumulative',    dest='cumulative', default=False, action='store_true',  help='Show the density or the cumulative distribution')

parser.add_argument('--any-node', dest='anyNode', default=False, action='store_true', help='Consider any note instead of the node with maximum latency')
parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--baseReliability', dest='baseReliability', default=1, type=float, help='Base reliability (i.e. the reliability obtained if we use the whole time range)')

parser.add_argument('--percentiles', dest='percentiles', default=[], type=float, nargs='+', help='Value for which to look the percentile')
parser.add_argument('--zoom', dest='zoom', type=float, nargs='+', help='Zoom with xmin and ymin')

parser.add_argument('logs', type=label_id_pair, help='List of logs')

pu.add_plot_arguments(parser)

args = parser.parse_args()

if args.zoom is not None and len(args.zoom) != 2:
    raise Exception('Zoom arguments should be 2 (xmin , ymin)')

print(args)

data = []

(l, jobs) = args.logs

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

        dd['job_id'] = job.id

        data.append(dd)

data = pd.concat(data)

del dd

if args.anyNode:
    d = data['diff']
else:
    d = data.groupby(['job_id', 'epoch', 'repetition_i'])['diff'].max()

labels = []

percentiles = [1 - 10**-i for i in range(1, 7)]

sorted(percentiles)

for p in percentiles:
    if args.baseReliability > p:
        t = (f'{p}', np.percentile(d, 100 * (p/args.baseReliability)))
        labels.append(t)

labels.extend([('{}'.format((d < p).mean()*args.baseReliability), p) for p in args.percentiles])
labels.extend([('mean', d.mean())])

perc_table = pd.DataFrame(labels, columns=['percentile', 'max_latency'])

print('Latency description\n{}'.format(d.describe()))
print('Percentile table:\n{}'.format(perc_table.sort_values('percentile')))

print(data.sort_values('diff', ascending=False).head(25))


def truncate_labels(v: float):
    v = str(v)

    try:
        cmin = min([v[2:].find(str(i)) for i in range(0, 9) if v[2:].find(str(i)) != -1])
    except ValueError:
        cmin = -1

    if cmin != -1:
        return v[:cmin+2+1] + ('' if len(v) <= cmin+2+1 else '…')
    else:
        return v


@pu.plot(args)
def plot():
    fig, ax = plt.subplots(1, 1)

    if not (args.xcm is not None and args.ycm is not None):
        if args.anyNode:
            ax.set_title('Distribution of latency inside an (epoch, repetition_i, node_id)')
        else:
            ax.set_title('Distribution of max latency inside an (epoch, repetition_i)')

    if args.cumulative:
        ax.hist(d, bins=100, cumulative=True, density=True)
    else:
        ax.hist(d, bins=100)

    ax.set_xlim((0, max(d)))

    if args.cumulative:
        ax.margins(y=0)

    ax.yaxis.set_major_formatter(matplotlib.ticker.PercentFormatter(xmax=1 if args.cumulative else len(d)))

    ax2 = ax.twiny()
    ax2.set_xlim(*ax.get_xlim())

    for l, v in labels:
        ax2.axvline(x=v, color='black', linestyle='-', linewidth=matplotlib.rcParams['axes.linewidth'])

    ax.set_xticks([v for l, v in labels], minor=False)
    ax.set_xticklabels([f'{round(v, 3):.3f}' for l, v in labels], rotation=-45, ha='left', minor=False)
    ax2.set_xticks([v for l, v in labels])
    ax2.set_xticklabels([truncate_labels(l) for l, v in labels], rotation=45, ha='left', minor=False)

    ax.xaxis.set_minor_locator(matplotlib.ticker.MaxNLocator(prune='lower'))
    ax.xaxis.set_minor_formatter('{x:.2f}')
    ax.xaxis.set_tick_params(direction='in', which='minor', pad=-15)

    if args.zoom is not None:
        axins = ax.inset_axes([0.03, 0.47, 0.30, 0.25])
        axins.hist(d, bins=100, cumulative=args.cumulative, density=True)

        axins2 = axins.twiny()

        for l, v in labels:
            axins2.axvline(x=v, color='black', linestyle='-', linewidth=matplotlib.rcParams['axes.linewidth'])

        axins.set_xticks([v for l, v in labels], minor=False)
        axins.set_xticklabels([f'{round(v, 3):.3f}' for l, v in labels], rotation=-45, ha='left', minor=False)
        axins2.set_xticks([v for l, v in labels])
        axins2.set_xticklabels([truncate_labels(l) for l, v in labels], rotation=45, ha='left', minor=False)

        axins.set_xlim(args.zoom[0], ax.get_xlim()[1])
        axins.set_ylim(args.zoom[1]/args.baseReliability, ax.get_ylim()[1])
        axins2.set_xlim(*axins.get_xlim())

        axins.tick_params(left=True, right=False, labelleft=False, labelbottom=True, bottom=True)

        ax.indicate_inset_zoom(axins, edgecolor='black')

        ax.set_xlabel(r'Latency [\si{\us}]')
        ax.set_ylabel('Percentage of positive FS')

    return fig

plot()
