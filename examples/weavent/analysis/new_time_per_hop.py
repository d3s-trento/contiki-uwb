#!/usr/bin/env python3

import argparse

import matplotlib.pyplot as plt
import common as c

import pandas as pd

import weavent_jobref as jr

import plot_utility as pu

def from_id(a: str) -> tuple[str, int]:
    return jr.WeaventLogId(int(a))

parser = argparse.ArgumentParser(
        description='Show how the latency is distributed wrt the nodes'
)

parser.add_argument('logs', type=from_id, nargs='+', help='the id of the logs to use')

parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--numNodes', dest='numNodes', default=33, type=int, help='Number of nodes in the network (excluding the sink)')

parser.add_argument('--guard', dest='guard', default=0.0, type=float, help='the rx_guard used')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

data = []

for job in args.logs:
    job: jr.WeaventLogId

    with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes, fp_guard=args.fpGuard) as a:
        dd = a.time_event_data
        epochData = a.bootstrap_data

        if args.epochFilter:
            eMin = epochData['epoch'].min() + 10
            eMax = epochData['epoch'].max() - 10

            dd = c.filter_by_epoch(dd, eMin, eMax)
            epochData = c.filter_by_epoch(epochData, eMin, eMax)

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

        dd = dd.merge(epochData, on=['node_id', 'epoch'], how='inner')

        print('Average number of hops: {}'.format(dd['hop'].mean()))

        first_hop = dd[dd['hop'] == 1].groupby('epoch').mean()['diff'].reset_index()

        dd = dd.merge(first_hop, on=['epoch'])
        dd['diff_first_hop'] = dd['diff_y']
        dd['diff'] = (dd['diff_x'] - dd['diff_first_hop']) / (dd['hop'] - 1)

        dd = dd[['node_id', 'epoch', 'repetition_i', 'detect_only', 'hop', 'diff', 'diff_first_hop']]

        data.append(dd)

data = pd.concat(data)

print('Average number of hops: {}'.format(data['hop'].mean()))

data = data[data['hop'] != 1]

print('Per node:\n{}'.format(data.groupby('node_id').describe()['diff']))
print('Aggregate:\n{}'.format(data.describe()['diff']))

@pu.plot(args, attach_subplots=True)
def plot():
    if args.xcm is not None and args.ycm is not None:
        plt.rcParams.update({
            'xtick.labelsize': 6,
            'ytick.labelsize': 6,
            'axes.labelsize':  7,
        })

    data['diff'] *= 1000

    fig, (axm, ax) = plt.subplots(nrows=1, ncols=2, sharey=True, gridspec_kw={'width_ratios': [2, 18]})

    axm.boxplot(data['diff'], sym='x', flierprops=dict(markersize=3, alpha=0.7))
    axm.violinplot(data['diff'], showextrema=False)

    axm.set_xticks([])
    axm.set_ylabel(r'Time per hop [\si{\us}]')

    nodes = sorted(data['node_id'].unique())

    w = [data[data['node_id'] == nid]['diff'] for nid in nodes]

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    ax.violinplot(w, showextrema=False)

    ax.set_xticklabels(nodes)
    ax.set_xlabel('node id')

    if not (args.xcm is not None and args.ycm is not None):
        ax.set_title('Latency per hop')

    return fig

plot()
