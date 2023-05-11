#!/usr/bin/env python3

import argparse

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

import jobref as jr

import plot_utility as pu

import filtering_utilities as fu

import itertools
import gc


def label_id_pair(a: str) -> tuple[str, int]:
    tmp = a.split(';', 3)

    if len(tmp) != 3:
        raise Exception('Should be 3 arguments')

    label, jobs_ids, jobs_ids2 = tmp

    return (label, [[jr.JobLogId(int(jid)) for jid in k.split('.')] for k in jobs_ids.split(',')], [[jr.JobLogId(int(jid)) for jid in k.split('.')] for k in jobs_ids2.split(',')])


parser = argparse.ArgumentParser(
        description='Show how the energy consumption is distributed'
)

parser.add_argument('jobs', type=label_id_pair, nargs='+',
                    help='the job_id/files to use')

parser.add_argument('-f', '--force', dest='force', action='store_true', default=False, help='Force recalculation')

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, False, None)
fu.add_completeEpoch_filter(parser, False, None)

parser.add_argument('--title', type=str, default='Energy consumption per epoch and node', help='Set title of plot')

parser.add_argument('--xlabel', type=str, default=None, help='Set the label to use for the x axis')
parser.add_argument('--ylabel', type=str, default=None, help='Set the label to use for the y axis')

parser.add_argument('--sink', dest='sink', default=True, action='store_true', help='Do not filter the sink')
parser.add_argument('--no-sink', dest='sink', default=True, action='store_false', help='Filter the sink')

parser.add_argument('--leftLabel', type=str, required=True)
parser.add_argument('--rightLabel', type=str, required=True)

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

data = []

def get_energy(job):
    optional_data = {}

    if args.completeEpochFilter:
        optional_data['num_nodes'] = args.numNodes

    if args.synchFilter:
        import project_jobref as pj

        job = pj.ProjectJobLogId(job.id)
        optional_data['num_nodes'] = args.numNodes

    with job.access(force=args.force, **optional_data) as a:
        df = a.energy

        df = fu.apply_epoch_filter(args, df)

        if args.synchFilter:
            complete_bootstrap = set(a.complete_bootstrap['epoch'])

            removed = set(df['epoch'].unique()) - complete_bootstrap

            if len(removed) > 0:
                print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, fu.pp_epochs(removed)))

            df = df[df['epoch'].isin(complete_bootstrap)]

        if args.completeEpochFilter:
            completed_epochs = set(a.completed_epochs['epoch'])

            removed = set(df['epoch'].unique()) - completed_epochs

            if len(removed) > 0:
                print('Filtering due incomplete epochs ({}): {}'.format(job.id, fu.pp_epochs(removed)))

            df = df[df['epoch'].isin(completed_epochs)]

    return df

for label, jids1, jids2 in args.jobs:
    gc.collect()

    left, right = jids1
    dds1 = (pd.concat([get_energy(job) for job in left]), pd.concat([get_energy(job) for job in right]))

    left, right = jids2
    dds2 = (pd.concat([get_energy(job) for job in left]), pd.concat([get_energy(job) for job in right]))

    label = int(label)

    dds1[0]['label'] = label
    dds1[1]['label'] = label

    dds2[0]['label'] = label
    dds2[1]['label'] = label

    data.append((label, dds1, dds2))

gc.collect()

for label, (l1, r1), (l2,r2) in data:
    print(f'{label}:')

    print(f'  {args.leftLabel}:')
    print('    0:')
    print(l1['e_total'].describe())
    print('    1:')
    print(r1['e_total'].describe())

    print(f'  {args.rightLabel}:')
    print('    0:')
    print(l2['e_total'].describe())
    print('    1:')
    print(r2['e_total'].describe())

l1 = pd.concat([dd for _, (dd, _), _ in data])
r1 = pd.concat([dd for _, (_, dd), _ in data])

l2 = pd.concat([dd for _, _, (dd, _) in data])
r2 = pd.concat([dd for _, _, (_, dd) in data])

if not args.sink:
    l1 = l1[l1['node_id'] != 119]
    r1 = r1[r1['node_id'] != 119]
    l2 = l2[l2['node_id'] != 119]
    r2 = r2[r2['node_id'] != 119]

def get_desc(df):
    t1 = df.groupby('label').describe()['e_total'].reset_index()
    t1 = t1.merge(df.groupby(['label', 'epoch']).agg({'e_total': ['min', 'max']}).groupby('label').mean(), on='label')
    t1 = t1.set_index('label')

    return t1

print(f'{args.leftLabel} L')
print(get_desc(l1))

print(f'{args.leftLabel} R')
print(get_desc(r1))

print(f'{args.rightLabel} L')
print(get_desc(l2))

print(f'{args.rightLabel} R')
print(get_desc(r2))

@pu.plot(args)
def plot():
    fig, ax = plt.subplots(1, 1)

    colors = ['tab:orange', 'tab:blue']

    labels = np.array([int(label) for label, _, _ in data])

    x = np.arange(0,labels.size)

    boxplot_options = dict(sym='x', flierprops=dict(markersize=3, alpha=0.7), widths=0.85, whis=[0.01, 0.99], showfliers=False)
    violin_options = dict(showextrema=False, widths=0.85)

    xleft = x*2.0-0.5
    xright = x*2.0+0.5

    for (l,r), label, color in [((l1,r1), args.leftLabel, colors[0]), ((l2,r2), args.rightLabel, colors[1])]:
        w1 = np.array([l[l['label'] == int(label)]['e_total'].mean() for label, _, _ in data])
        w2 = np.array([r[r['label'] == int(label)]['e_total'].mean() for label, _, _ in data])

        print(w1/w2)

        ax.plot(2*x, w1/w2, color=color, label=f'{label}')

    ax.grid(True)

    #vp = ax.violinplot(w, **violin_options, positions=[xright], label=args.rightLabel)

    #for body in vp['bodies']:
    #    body.set_facecolor(color)

    ax.set_xticks(2*x)
    ax.set_xticklabels(labels)
    ax.set_xlabel('U')

    ax.set_ylabel(r'Ratio on energy consumption')

    if not (args.xcm is not None and args.ycm is not None):
        ax.set_title(args.title)

    if args.xlabel is not None:
        ax.set_xlabel(args.xlabel)

    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=2, bbox_to_anchor=(0.5, 1))
    fig.suptitle(' ')

    return fig

plot()
