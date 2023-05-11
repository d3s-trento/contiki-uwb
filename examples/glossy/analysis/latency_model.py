#!/usr/bin/env python3

import argparse

import matplotlib.pyplot as plt

import pandas as pd
import numpy as np

import glossy_jobref as jr

import plot_utility as pu
import filtering_utilities as fu

import gc

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.GlossyLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser(
        description='Show how the latency is distributed wrt the nodes'
)

parser.add_argument('logs', type=label_id_pair, nargs='+', help='the id of the logs to use')

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('-hst', '--hop-stability-threshold', dest='hst', default=0.9, type=float, help='Threshold after which we filter out nodes as non-stable')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

data = dict()

for label, jobs in args.logs:
    for job in jobs:
        job: jr.GlossyLogId

        with job.access(args.force, num_nodes=args.numNodes) as a:
            dd = a.latency
            epochData = a.bootstrap_data

            dd, epochData = fu.apply_epoch_filter(args, dd, epochData)

            if args.synchFilter:
                complete_bootstrap = set(a.complete_bootstrap['epoch'])

                removed = set(dd['epoch'].unique()) - complete_bootstrap

                if len(removed) > 0:
                    print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, fu.pp_epochs(removed)))

                dd = dd[dd['epoch'].isin(complete_bootstrap)]
                epochData = epochData[epochData['epoch'].isin(complete_bootstrap)]

            if args.completeEpochFilter:
                completed_epochs = set(a.completed_epochs['epoch'])

                removed = set(dd['epoch'].unique()) - completed_epochs

                if len(removed) > 0:
                    print('Filtering due incomplete epochs ({}): {}'.format(job.id, fu.pp_epochs(removed)))

                dd = dd[dd['epoch'].isin(completed_epochs)]
                epochData = epochData[epochData['epoch'].isin(completed_epochs)]

            nodes_hop = epochData[['node_id', 'hop']].groupby('node_id')['hop'].agg(pd.Series.mode).reset_index()

            tmp = epochData[['node_id', 'hop']].merge(nodes_hop, on='node_id')
            tmp['same'] = tmp['hop_x'] == tmp['hop_y']

            nodes_hop = nodes_hop.merge(tmp.groupby('node_id').mean()['same'].reset_index(), on='node_id')

            removed = set(nodes_hop[nodes_hop['same'] < args.hst]['node_id'])

            if len(removed) > 0:
                print(f'Removing all the data from nodes for hop instability: {removed}')

            nodes_hop = nodes_hop[nodes_hop['same'] >= args.hst]

            dd = dd[~dd['node_id'].isin(removed)]
            epochData = epochData[~epochData['node_id'].isin(removed)]

            dd['id'] = job.id
            dd = dd.merge(nodes_hop[['node_id', 'hop']], on=['node_id'], how='inner')

            dd['duration']/=1e6

            first_hop = dd[dd['hop'] == 1].groupby('epoch').mean()['duration'].reset_index()

            dd = dd.merge(first_hop, on=['epoch'])
            dd['diff'] = dd['duration_x']

            dd = dd[['node_id', 'epoch', 'repetition_i', 'hop', 'diff']]

            if label not in data:
                data[label] = [dd]
            else:
                data[label].append(dd)

gc.collect()

import scipy.stats

for label, _ in args.logs:
    data_l = pd.concat(data[label])

    del data[label]

    print(f'{label}:')
    print('  Average number of hops: {}'.format(data_l['hop'].mean()))
    hmax = data_l.groupby(['epoch', 'repetition_i'])['hop'].max().quantile(0.98)
    print(f'  Hmax {hmax}')
    print('  Average max distance from sink: {}'.format(data_l.groupby(['epoch', 'repetition_i'])['hop'].max().mean()))

    non_final = data_l[data_l['hop'] <= hmax-1]

    del non_final

    data_l = data_l[['diff', 'hop']]

    rr = data_l.groupby('hop')['diff'].agg([
        lambda x: x.mean(),
        lambda x: x.median(),
        lambda x: x.quantile(0.9),
        lambda x: x.quantile(0.99),
        lambda x: x.quantile(0.999),
        lambda x: x.quantile(0.9999),
        lambda x: x.quantile(0.99999)
    ]).reset_index()

    rr.columns = ['hop', 'mean', '0.5', '0.9', '0.99', '0.999', '0.9999', '0.99999']

    rrDiff = rr.set_index('hop').diff().dropna(axis=0)

    fhl = rr[rr['hop'] == 1]['0.5'].iloc[0]
    lph = rrDiff['0.5'].mean()

    fhl -= lph

    print(f'  Offset: {fhl*1000} us')
    print(f'  Slope: {lph*1000} us')
    print(f'diff description\n{rr}')
    print(f'diff description (delta)\n{rr.diff()}')

    data[label] = (data_l, hmax, fhl, lph, data_l['hop'].max(), rr)

    del data_l

    gc.collect()

@pu.plot(args)
def plot():
    sizes = np.array([m for _, (_, _, _, _, m, _) in data.items()])

    total = (np.sum(sizes))

    fig, axs = plt.subplots(
            nrows=1,
            ncols=len(data.keys()),
            gridspec_kw={'width_ratios': sizes, 'wspace': 0.025},
            sharey=True)

    axs = list(axs) if isinstance(axs, np.ndarray) else [axs]

    colors = ['tab:orange', 'tab:red', 'tab:green', 'tab:blue']

    for (label, _), ax, size, color in zip(args.logs, axs, sizes, colors):
        data_l, hmax, fhl, lph, _, rr = data[label]

        if not args.save:
            ax.set_title(label, y=0.95, va="top")

            txt = f'Offset: {fhl*1000} us\n' +\
                  f'Slope: {lph*1000} us'

            ax.text(0.02, 0.98, txt, fontsize=12, transform=ax.transAxes, va='top')

        hops = np.array(range(1, data_l['hop'].max()+1))

        w = [data_l[data_l['hop'] == i]['diff']*1000 for i in hops]

        ax.boxplot(w, sym='x', whis=(0.01, 0.99), flierprops=dict(markersize=3, alpha=0.7), showfliers=False, medianprops=dict(color=color), widths=0.85)

        if not args.save:
            vp = ax.violinplot(w, showextrema=False, widths=0.85)

            for body in vp['bodies']:
                body.set_facecolor(color)

        #for k in ['0.99999']:
        #    ax.plot(rr['hop'], rr[k]*1000, label=k, color='black', linewidth=0.5)

        ax.plot(hops, 1000*(fhl + lph*(hops)), linestyle='solid', label='Per-hop latency trend', color=color)

        ax.set_xticklabels(hops)
        ax.set_xlabel('Hop distance')

        ax.grid(alpha=0.2)

        #ax.set_xlim((0.5, 0.5+size+0.5))

        ax.tick_params(left=False)  # remove the ticks

    axs[0].tick_params(left=True)  # re-add the ticks only for the left plot
    axs[0].tick_params(axis='y', labelrotation=55, pad=-1)
    # axs[0].set_ylabel(r'Latency [\si{\us}]')

    # handles, labels = axs[0].get_legend_handles_labels()
    # fig.legend(handles, labels, loc='upper center', ncol=3)#, bbox_to_anchor=(0.5, 1))
    # fig.suptitle(' ')
    #axs[0].legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2 if args.xcm > 15 else 1)

    if not (args.xcm is not None and args.ycm is not None):
        axs[0].set_title('Latency per hop')

    return fig

plot()
