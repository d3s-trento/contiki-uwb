#!/usr/bin/env python3

import argparse

import pandas as pd
import matplotlib.pyplot as plt

import statetime
import jobref as jr

import numpy as np
import scipy.stats

import plot_utility as pu

import filtering_utilities as fu

import gc

import os


def label_id_pair(a: str) -> tuple[str, int]:
    tmp = a.split(';', 2)

    print(tmp)

    if len(tmp) > 1:
        label = tmp[0]
        job_ids = tmp[1]
    else:
        label = tmp[0]
        job_ids = tmp[0]

    if len(tmp) > 2:
        f = tmp[2]
    else:
        f = None

    return (label, [jr.JobLogId(int(jid)) for jid in job_ids.split(';')], f)


parser = argparse.ArgumentParser(
        description='Show how the energy consumption is distributed'
)

parser.add_argument('logs', type=label_id_pair, nargs='+',
                    help='the job_id/files to use')

parser.add_argument('-f', '--force', dest='force', action='store_true', default=False, help='Force recalculation')

parser.add_argument('-hst', '--hop-stability-threshold', dest='hst', default=0.9, type=float, help='Threshold after which we filter out nodes as non-stable')

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, False, None)
fu.add_completeEpoch_filter(parser, False, None)

parser.add_argument('--base', type=int, nargs='+', default=[0], help='Base to subtract')

parser.add_argument('--title', type=str, default='Energy consumption per epoch and node', help='Set title of plot')

parser.add_argument('--divide', type=int, nargs='+', default=[1], help='Number of iteration (divides the results by this number)')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

if len(args.base) == 1:
    args.base = [args.base]*len(args.logs)
elif len(args.base) != len(args.logs):
    import sys
    print('Error: base and job list are of different lengths')

    sys.exit(1)

if len(args.divide) == 1:
    args.divide = [args.divide]*len(args.logs)
elif len(args.divide) != len(args.logs):
    import sys
    print('Error: divide and job list are of different lengths')

    sys.exit(1)

data = dict()

bak = (os.environ['PROJECT_DIR'], os.environ['PATH'], os.environ['JOBS_DIR'], os.environ['PYTHONPATH'])

for b, (label, p, f), div in zip(args.base, args.logs, args.divide):
    os.environ['PROJECT_DIR'], os.environ['PATH'], os.environ['JOBS_DIR'], os.environ['PYTHONPATH'] = bak

    if f is not None:
        os.environ['PROJECT_DIR'] = f
        os.environ['PATH'] = os.environ['PATH'] + ':' + os.environ['PROJECT_DIR'] + '/tools'
        os.environ['JOBS_DIR'] = os.environ['PROJECT_DIR'] + '/jobs'
        os.environ['PYTHONPATH'] = os.environ['PYTHONPATH'] + ':' + os.environ['PROJECT_DIR'] + '/analysis'

    pj = __import__('project_jobref')

    for job in p:
        optional_data = {}

        if args.completeEpochFilter:
            optional_data['num_nodes'] = args.numNodes

        if args.synchFilter:
            job = pj.ProjectJobLogId(job.id)

            optional_data['num_nodes'] = args.numNodes

        with job.access(force=args.force, **optional_data) as a:
            df = a.energy
            epochData = a.bootstrap_data

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

            nodes_hop = epochData[['node_id', 'hop']].groupby('node_id')['hop'].agg(pd.Series.mode).reset_index()

            tmp = epochData[['node_id', 'hop']].merge(nodes_hop, on='node_id')
            tmp['same'] = tmp['hop_x'] == tmp['hop_y']

            nodes_hop = nodes_hop.merge(tmp.groupby('node_id').mean()['same'].reset_index(), on='node_id')

            removed = set(nodes_hop[nodes_hop['same'] < args.hst]['node_id'])

            if len(removed) > 0:
                print(f'Removing all the data from nodes for hop instability: {removed}')

            nodes_hop = nodes_hop[nodes_hop['same'] >= args.hst]

            df = df[~df['node_id'].isin(removed)]
            epochData = epochData[~epochData['node_id'].isin(removed)]

            df = df.merge(nodes_hop[['node_id', 'hop']], on=['node_id'], how='inner')

            df['e_total'] -= b
            df['e_total'] /= div

            df['name'] = label

            if label not in data:
                data[label] = [df]
            else:
                data[label].append(df)

gc.collect()

import scipy.stats

for label, _, _ in args.logs:
    data_l = pd.concat(data[label])

    del data[label]

    print(f'{label}:')
    print('  Average number of hops: {}'.format(data_l['hop'].mean()))
    hmax = data_l.groupby(['epoch'])['hop'].max().quantile(0.98)
    print(f'  Hmax {hmax}')
    print('  Average max distance from sink: {}'.format(data_l.groupby(['epoch'])['hop'].max().mean()))

    # This commented part simply uses too much ram for amount I have
    #print('  Per node:\n{}'.format(data_l[data_l['hop'] != 1].groupby('node_id').describe()['diff_per_hop']))

    non_final = data_l[data_l['hop'] <= hmax-1]

    del non_final

    data_l = data_l[['e_total', 'hop']]

    rr = data_l.groupby('hop')['e_total'].agg([
        lambda x: x.mean(),
        lambda x: x.median(),
        lambda x: x.quantile(0.9),
        lambda x: x.quantile(0.99),
        lambda x: x.quantile(0.999),
        lambda x: x.quantile(0.9999),
        lambda x: x.quantile(0.99999)
    ]).reset_index()

    rr.columns = ['hop', 'mean', '0.5', '0.9', '0.99', '0.999', '0.9999', '0.99999']

    rrDiff = rr[rr['hop'] >= 1].set_index('hop').diff().dropna(axis=0)

    fhl = rr[rr['hop'] == 1]['0.5'].iloc[0]
    lph = rrDiff['0.5'].mean()

    fhl -= lph

    pd.set_option("display.max_colwidth", None, "display.width", None)

    print(f'  Offset: {fhl} uJ')
    print(f'  Slope: {lph} uJ')
    print(f'energy description\n{rr}')
    print(f'energy description (delta)\n{rr.diff()}')

    if args.plot:
        data[label] = (data_l, hmax, fhl, lph, data_l['hop'].max(), rr)

    del data_l

    gc.collect()

@pu.plot(args)
def plot():
    sizes = np.array([m for _, (_, _, _, _, m, _) in data.items()])

    total = (np.sum(sizes))

    colors = ['tab:green', 'tab:blue', 'tab:orange', 'tab:red']

    fig, ax = plt.subplots(nrows=1, ncols=1)

    for (label, _, _), color in zip(args.logs, colors):
        data_l, hmax, fhl, lph, _, rr = data[label]

        if not args.save:
            ax.set_title(label, y=0.95, va="top")

            txt = f'Offset: {fhl} uJ\n' +\
                  f'Slope: {lph} uJ'

            ax.text(0.02, 0.98, txt, fontsize=12, transform=ax.transAxes, va='top')

        hops = np.array(range(1, data_l['hop'].max()+1))

        w = [data_l[data_l['hop'] == i]['e_total'] for i in hops]

        ax.boxplot(w, sym='x', whis=(0.01, 0.99), flierprops=dict(markersize=3, alpha=0.7), showfliers=False, medianprops=dict(color=color), widths=0.85)

        if not args.save:
            vp = ax.violinplot(w, showextrema=False)

            for body in vp['bodies']:
                body.set_facecolor(color)

        #for k in ['0.99999']: #list(rr.columns)[1:]:
        #    ax.plot(rr['hop'], rr[k]*1000, label=k, color='black', linewidth=0.5)

        ax.plot(hops, (fhl + lph*(hops)), linestyle='solid', label=label, color=color)

        #ax.set_xticklabels(hops)
        ax.set_xlabel('Hop distance')

        ax.grid(alpha=0.2)

    ax.tick_params(left=True)  # re-add the ticks only for the left plot
    ax.set_ylabel(r'Energy [\si{\uJ}]')

    handles, labels = ax.get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=2, bbox_to_anchor=(0.5, 1))
    fig.suptitle(' \n ')
    #axs[0].legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2 if args.xcm > 15 else 1)

    if not (args.xcm is not None and args.ycm is not None):
        ax.set_title('Energy consumption per hop')

    return fig

plot()
