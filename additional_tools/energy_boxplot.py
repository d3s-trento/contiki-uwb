#!/usr/bin/env python3

import argparse

import pandas as pd
import matplotlib.pyplot as plt

import statetime
import jobref as jr

import plot_utility as pu

import filtering_utilities as fu


def label_id_pair(a: str) -> tuple[str, int]:
    tmp = a.split(';', 1)

    if len(tmp) > 1:
        label, job_ids = tmp
    else:
        label, job_ids = (tmp[0], tmp[0])

    return (label, [jr.JobLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser(
        description='Show how the energy consumption is distributed'
)

parser.add_argument('jobs', type=label_id_pair, nargs='+',
                    help='the job_id/files to use')

parser.add_argument('-n', '--separateNodes', dest='nodeSep', default=False, action='store_true', help='Separate the data of the different nodes')

parser.add_argument('-f', '--force', dest='force', action='store_true', default=False, help='Force recalculation')

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, False, None)
fu.add_completeEpoch_filter(parser, False, None)

parser.add_argument('--base', type=int, nargs='+', default=[0], help='Base to subtract')

parser.add_argument('--title', type=str, default='Energy consumption per epoch and node', help='Set title of plot')

parser.add_argument('--xlabel', type=str, default=None, help='Set the label to use for the x axis')
parser.add_argument('--ylabel', type=str, default=None, help='Set the label to use for the y axis')

parser.add_argument('--divide', type=int, default=1, help='Number of iteration (divides the results by this number)')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

if len(args.base) == 1:
    args.base = [args.base]*len(args.jobs)
elif len(args.base) != len(args.jobs):
    import sys
    print('Error: base and job list are of different lengths')

    sys.exit(1)

data = []

for b, (label, p) in zip(args.base, args.jobs):
    for job in p:
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

            df['e_total'] -= b
            df['name'] = label

            data.append(df)

data = pd.concat(data)

data['e_total'] /= args.divide

print(data.groupby('name').describe()['e_total'])

@pu.plot(args)
def plot():
    fig, ax = plt.subplots(1, 1)

    if not args.nodeSep:
        w = [data[data['name'] == label]['e_total'] for label, _ in args.jobs]
    else:
        w = [data[(data['name'] == label) & (data['node_id'] == nid)]['e_total'] for label, _ in args.jobs for nid in data['node_id'].unique()]

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax.violinplot(w, showextrema=False)

    if not args.nodeSep:
        #ax.set_xticklabels([r'\begin{center}'+label+r'\end{center}' for label, _ in args.jobs])
        ax.set_xticklabels([label for label, _ in args.jobs])
    else:
        ax.set_xticklabels([f'{n}\n{nid}' for label, _ in args.jobs for nid in data['node_id'].unique()])

    ax.set_ylabel(args.ylabel or r'Energy consumption per epoch and node [\si{\uJ}]')

    if not (args.xcm is not None and args.ycm is not None):
        ax.set_title(args.title)

    if args.xlabel is not None:
        ax.set_xlabel(args.xlabel)

    return fig

plot()
