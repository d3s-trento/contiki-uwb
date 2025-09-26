#!/usr/bin/env python3

import argparse

import matplotlib.pyplot as plt
import common as c

import pandas as pd
import numpy as np

import weavent_jobref as jr

import plot_utility as pu
import filtering_utilities as fu

import gc

# def from_id(a: str) -> tuple[str, int]:
#     return jr.WeaventLogId(int(a))

def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.WeaventLogId(int(jid)) for jid in job_ids.split(';')])


parser = argparse.ArgumentParser(
        description='Show how the latency is distributed wrt the nodes'
)

parser.add_argument('logs', type=label_id_pair, nargs='+', help='the id of the logs to use')

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')

parser.add_argument('--fp-guard', dest='fpGuard', default=0.2, type=float, help='Guard after which false positives are filtered out')

parser.add_argument('--guard', dest='guard', default=0.0, type=float, help='the rx_guard used')

parser.add_argument('--bad-frame', dest='badFrame', default=False, action='store_true', help='Differentiate between bad frames and preamble detection')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

data = dict()

for label, jobs in args.logs:
    for job in jobs:
        job: jr.WeaventLogId

        with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes, fp_guard=args.fpGuard) as a:
            dd = a.time_event_data
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

            if args.filterFalsePositives:
                falsePositives = a.false_positives

                if len(falsePositives) > 0:
                    print('Removing false postives ({}):\n {}'.format(job.id, falsePositives))

                dd = dd.merge(falsePositives[['node_id', 'epoch', 'repetition_i']], how='outer', on=['node_id', 'epoch', 'repetition_i'], indicator=True)
                dd = dd[dd._merge == 'left_only']

            dd['id'] = job.id
            dd = dd.merge(epochData, on=['node_id', 'epoch'], how='inner')

            first_hop = dd[dd['hop'] == 1].groupby('epoch').mean()['diff'].reset_index()

            dd = dd.merge(first_hop, on=['epoch'])
            dd['diff_first_hop'] = dd['diff_y']
            dd['diff_per_hop'] = (dd['diff_x'] - dd['diff_first_hop']) / (dd['hop'] - 1)
            dd['diff'] = dd['diff_x']

            dd = dd[['node_id', 'epoch', 'repetition_i', 'detect_only', 'hop', 'diff', 'diff_per_hop']]

            if label not in data:
                data[label] = [dd]
            else:
                data[label].append(dd)

gc.collect()

for label, _ in args.logs:
    data_l = pd.concat(data[label])

    del data[label]

    print(f'{label}:')
    print('  Average number of hops: {}'.format(data_l['hop'].mean()))
    hmax = data_l.groupby(['epoch', 'repetition_i'])['hop'].max().quantile(0.98)
    print(f'  Hmax {hmax}')

    print('  Average max distance from sink: {}'.format(data_l.groupby(['epoch', 'repetition_i'])['hop'].max().mean()))

    fhl = data_l[data_l['hop'] == 1]['diff'].mean()
    lph = data_l[(data_l['hop'] != 1) & (data_l['hop'] <= hmax-1)]['diff_per_hop'].mean()

    print(f'  First hop latency: {fhl*1000} us')
    print(f'  Mean per hop latency: {lph*1000} us')

    # This commented part simply uses too much ram for amount I have
    #print('  Per node:\n{}'.format(data_l[data_l['hop'] != 1].groupby('node_id').describe()['diff_per_hop']))

    non_final = data_l[data_l['hop'] <= hmax-1]
    print('Outside maximum for intermediate hop: {}%'.format((non_final['diff'] > (fhl + lph*(non_final['hop']+1) + 0.07232)).mean()*100))

    del non_final

    print('Outside maximum for final hop: {}%'.format((data_l['diff'] > (fhl + lph*hmax + 0.07232)).mean()*100))
    print('RMSE wrt expected/modeled mean:', np.sqrt(((data_l['diff'] - (fhl + lph*(data_l['hop']-1)))**2).mean()))

    data_l = data_l[['diff', 'hop', 'detect_only']]
    data[label] = (data_l, hmax, fhl, lph, data_l['hop'].max())

    del data_l

    gc.collect()

@pu.plot(args)
def plot():
    sizes = np.array([m for _, (_, _, _, _, m) in data.items()])

    total = (np.sum(sizes))

    fig, axs = plt.subplots(
            nrows=1,
            ncols=len(data.keys()),
            gridspec_kw={'width_ratios': sizes, 'wspace': 0.025},
            sharey=True)

    axs = list(axs) if isinstance(axs, np.ndarray) else [axs]

    for (label, _), ax, size in zip(args.logs, axs, sizes):
        data_l, hmax, fhl, lph, _ = data[label]

        ax.set_title(label, y=0.95, va="top")

        if not args.save:
            txt = f'First hop latency: {fhl*1000} us\n' +\
                  f'Mean per hop latency: {lph*1000} us'

            ax.text(0.02, 0.98, txt, fontsize=12, transform=ax.transAxes, va='top')

        hops = np.array(range(1, data_l['hop'].max()+1))

        w = [data_l[data_l['hop'] == i]['diff']*1000 for i in hops]

        bx = ax.boxplot(w, sym='x', whis=(0.01, 0.99), flierprops=dict(markersize=3, alpha=0.7), showfliers=not args.badFrame)
        ax.violinplot(w, showextrema=False)

        if args.badFrame:
            caps = dict()
            tmp = [(int(round((item.get_xdata()[0]+item.get_xdata()[1])/2)), item.get_ydata()[0]) for item in bx['caps']]
            for h in hops:
                caps[h] = sorted([y for x,y in tmp if x == h])

            for h in hops:
                tmp = data_l
                tmp = tmp[tmp['hop'] == h]
                tmp = tmp[~((tmp['diff']*1000).between(caps[h][0], caps[h][1]))]

                from matplotlib.colors import LinearSegmentedColormap

                ax.scatter(tmp['hop'], tmp['diff']*1000, marker='x', c=tmp['detect_only'], linewidth=1, s=5, alpha=0.7, cmap=LinearSegmentedColormap.from_list("mycmap", [(0,'black'), (1,'red')]))

        ax.plot(hops, 1000*(fhl + lph*(hops-1)), linestyle='solid', label='Mean')
        ax.plot(hops, 1000*(fhl + lph*(hops+1) + 0.07232), linestyle='dashdot', label='Max. per hop') # TODO: Check 72.32us

        ax.axhline(1000*(fhl + lph * hmax + 0.07232), color='red', linestyle='dotted', label='Max.')

        ax.set_xticklabels(hops)
        ax.set_xlabel('Hop distance')

        ax.grid(alpha=0.2)

        ax.set_xlim((0.5, 0.5+size+0.5))

        ax.tick_params(left=False)  # remove the ticks

    axs[0].tick_params(left=True)  # re-add the ticks only for the left plot
    axs[0].set_ylabel(r'Latency [\si{\us}]')

    handles, labels = axs[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc='upper center', ncol=3, bbox_to_anchor=(0.5, 1))
    fig.suptitle(' ')
    #axs[0].legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2 if args.xcm > 15 else 1)

    if not (args.xcm is not None and args.ycm is not None):
        axs[0].set_title('Latency per hop')

    return fig

plot()
