#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

import itertools

import numpy as np

import argparse

import weavent_jobref as jr

import plot_utility as pu
import filtering_utilities as fu


def from_job(a: str):
    return jr.WeaventLogId(int(a))


parser = argparse.ArgumentParser(
        description='Calculate the reliability of the event flood when there is already a network-wide successful sync flood'
)

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--no-filterFalsePositives', dest='filterFalsePositives', default=False, action='store_false', help='filter out probable false positives')
parser.add_argument('--filterFalsePositives',    dest='filterFalsePositives', default=False, action='store_true',  help='filter out probable false positives')
parser.add_argument('--fp-guard', dest='fpGuard', type=float, default=0.2, help='Guard used to filter out false postives')

parser.add_argument('--slot_idx', dest='slot_idx', action='store_true', default=False, help='Use slot_idx as the x axis')

parser.add_argument('--repetitions', dest='repetitions', default=100, help='Set the number of repetitions to take into account')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

pu.add_plot_arguments(parser)

parser.add_argument('log', type=from_job, nargs='+', help='the ids of the log to use')

args = parser.parse_args()

print(args)

ff_data = []

for job in args.log:
    job: jr.WeaventLogId

    with job.access(force=args.force, num_nodes=args.numNodes, fp_guard=args.fpGuard) as a:
        dd = a.time_event_data
        tx_fs = a.tx_fs

        boot = a.bootstrap_data
        tsm = a.tsm_slots

        epochList = set(dd['epoch']).union(set(tx_fs['epoch'])).union(set(boot['epoch'])).union(set(tsm['epoch']))

        epochList = set(range(min(epochList), max(epochList)+1))

        del boot
        del tsm

        epochList = pd.Series(list(epochList)).to_frame(name='epoch')

        epochList = fu.apply_epoch_filter(args, epochList)

        epochList = set(epochList['epoch'])

        dd = dd[dd['epoch'].isin(epochList)]
        tx_fs = tx_fs[tx_fs['epoch'].isin(epochList)]

        ##dd, tx_fs = fu.apply_epoch_filter(args, dd, tx_fs)

        ##epochList = set(dd['epoch'].unique()).union(set(tx_fs['epoch'].unique()))
        #epochList = set(list(range(min(dd['epoch'].min(), tx_fs['epoch'].min()), max(dd['epoch'].max(), tx_fs['epoch'].max()))))

        if args.synchFilter:
            complete_bootstrap = set(a.complete_bootstrap['epoch'])

            removed = epochList - complete_bootstrap

            if len(removed) > 0:
                print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, sorted(removed)))

            dd = dd[dd['epoch'].isin(complete_bootstrap)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(complete_bootstrap)]

            epochList = epochList.intersection(complete_bootstrap)

        if args.completeEpochFilter:
            completed_epochs = set(a.completed_epochs['epoch'])

            removed = epochList - completed_epochs

            if len(removed) > 0:
                print('Filtering due incomplete epochs ({}): {}'.format(job.id, sorted(removed)))

            dd = dd[dd['epoch'].isin(completed_epochs)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(completed_epochs)]

            epochList = epochList.intersection(completed_epochs)

        if args.filterFalsePositives:
            falsePositives = a.false_positives

            if len(falsePositives) > 0:
                print('Removing false postives ({}):\n {}'.format(job.id, falsePositives))

            dd = dd.merge(falsePositives[['node_id', 'epoch', 'repetition_i']], how='outer', on=['node_id', 'epoch', 'repetition_i'], indicator=True)
            dd = dd[dd._merge == 'left_only']


        ed = dd.groupby(['epoch', 'repetition_i']).size().reset_index()
        ed['positive_floods'] = 1

        tx = tx_fs.groupby(['epoch', 'repetition_i']).size().reset_index()

        finalData = pd.merge(ed, tx, on=['epoch', 'repetition_i'], how='outer').fillna(0)

        base = pd.DataFrame(itertools.product(epochList, range(0, args.repetitions)), columns=['epoch', 'repetition_i'])
        finalData = pd.merge(finalData, base, how='outer', on=['epoch', 'repetition_i']).fillna(0)

        finalData['rx_nodes'] = a.num_nodes + 1 - finalData['0_y']
        finalData['total_nodes'] = a.num_nodes + 1

        finalData['receptions'] = finalData['0_x']
        finalData['positives'] = finalData['0_x'] + finalData['0_y']

        finalData['floods'] = 1

        finalData = finalData[['epoch', 'repetition_i', 'rx_nodes', 'total_nodes', 'receptions', 'positives', 'positive_floods', 'floods']]

        print('info: Epochs (in job_{}): {}'.format(job.id, finalData.nunique()['epoch']))

        issueMask = (finalData['positives'] > finalData['receptions'])

        if len(finalData[issueMask]) > 0:
            print('ERROR: Someone acted as originator ERROR (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        issueMask = (finalData['receptions'] > finalData['rx_nodes']) | (finalData['positives'] > finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('ERROR: Reliability >1 ERROR (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        issueMask = (finalData['receptions'] > 0) | (finalData['positives'] > 0)

        if len(finalData[issueMask]) > 0:
            print('Reliability <1 warning (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        finalData = finalData.groupby('repetition_i').sum().reset_index()

        finalData = finalData[['repetition_i', 'rx_nodes', 'total_nodes', 'receptions', 'positives', 'positive_floods', 'floods']]

        issueMask = (finalData['receptions'] > finalData['rx_nodes']) | (finalData['positives'] > finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('ERROR: Reliability >1 ERROR (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        issueMask = (finalData['receptions'] > 0) | (finalData['positives'] > 0)

        if len(finalData[issueMask]) > 0:
            print('Reliability <1 warning (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        if len(finalData['repetition_i'].unique()) != args.repetitions:
            print(f'There are less than {args.repetitions} repetition_i')

        finalData = finalData[['repetition_i', 'receptions', 'positives', 'rx_nodes', 'total_nodes', 'positive_floods', 'floods']]

    del dd
    del tx_fs

    import gc
    gc.collect()

    ff_data.append(finalData)

ff_data = pd.concat(ff_data)
ff_data['slot_idx'] = 9 + 4 * ff_data['repetition_i']

ff_data = ff_data.groupby(['repetition_i', 'slot_idx']).sum().reset_index()

print('total reliability(f): {} {}/{}'.format(ff_data['positive_floods'].sum()/ff_data['floods'].sum(), ff_data['positive_floods'].sum(), ff_data['floods'].sum()))
print('total reliability: {} {}/{}'.format(ff_data['positives'].sum()/ff_data['total_nodes'].sum(), ff_data['positives'].sum(), ff_data['total_nodes'].sum()))

ff_data['p_reception_reliability'] = (ff_data['receptions'] / ff_data['rx_nodes']) * 100 # 100 as 100%
ff_data['p_total_reliability'] = (ff_data['positives'] / ff_data['total_nodes']) * 100 # 100 as 100%

@pu.plot(args)
def plot():
    x_lab = 'repetition_i' if not args.slot_idx else 'slot_idx'

    fig, ax= plt.subplots(1, 1)
    ax.plot(ff_data[x_lab], ff_data['p_total_reliability'],     label='Total reliability')
    ax.plot(ff_data[x_lab], ff_data['p_reception_reliability'], label='Reception reliability')

    for s, m, line in zip(['-', '--'], ['x', '.'], ax.get_lines()):
        line.set_marker(m)
        line.set_linestyle(s)

    ax.set_ylabel(r'Reliability [\%]')
    ax.yaxis.set_major_formatter(mtick.PercentFormatter())

    ax.set_xticks(list(range(0, args.repetitions)) if not args.slot_idx else (9+np.arange(0, args.repetitions)))
    ax.set_xlabel('repetitions' if not args.slot_idx else 'slot index')

    ax.legend(bbox_to_anchor=(0.5, 1), loc='lower center', ncol=2)

    return fig

plot()
