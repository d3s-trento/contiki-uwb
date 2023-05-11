#!/usr/bin/env python3

import common as c

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

        nodesList = set(dd['node_id']).union(set(tx_fs['node_id'])).union(set(boot['node_id'])).union(set(tsm['node_id']))

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
                print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, fu.pp_epochs(removed)))

            dd = dd[dd['epoch'].isin(complete_bootstrap)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(complete_bootstrap)]

            epochList = epochList.intersection(complete_bootstrap)

        if args.completeEpochFilter:
            completed_epochs = set(a.completed_epochs['epoch'])

            removed = epochList - completed_epochs

            if len(removed) > 0:
                print('Filtering due incomplete epochs ({}): {}'.format(job.id, fu.pp_epochs(removed)))

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
        tx = tx_fs.groupby(['epoch', 'repetition_i']).size().reset_index()

        finalData = pd.merge(ed, tx, on=['epoch', 'repetition_i'], how='outer').fillna(0)

        base = pd.DataFrame(itertools.product(epochList, range(0, args.repetitions)), columns=['epoch', 'repetition_i'])
        finalData = pd.merge(finalData, base, how='outer', on=['epoch', 'repetition_i']).fillna(0)

        finalData['rx_nodes'] = a.num_nodes + 1 - finalData['0_y']
        finalData['total_nodes'] = a.num_nodes + 1

        finalData['receptions'] = finalData['0_x']
        finalData['positives'] = finalData['0_x'] + finalData['0_y']

        finalData = finalData[['epoch', 'repetition_i', 'rx_nodes', 'total_nodes', 'receptions', 'positives']]

        print('info: Epochs (in job_{}): {}'.format(job.id, finalData.nunique()['epoch']))

        issueMask = (finalData['receptions'] < finalData['rx_nodes']) | (finalData['positives'] < finalData['total_nodes'])

        problematicFloods = finalData[issueMask]

        interestedData = dd.merge(problematicFloods, how='inner')
        interestedOriginators = tx_fs.merge(problematicFloods, how='inner')

        problematicFloods = problematicFloods.merge(interestedData.groupby(['epoch', 'repetition_i']).agg({'node_id': set}).reset_index(), how='outer', on=['epoch', 'repetition_i'])
        problematicFloods.rename(columns={'node_id': 'received'}, inplace=True)

        print(problematicFloods)

        problematicFloods = problematicFloods.merge(interestedOriginators.groupby(['epoch', 'repetition_i']).agg({'node_id': set}).reset_index(), how='outer', on=['epoch', 'repetition_i'])
        problematicFloods.rename(columns={'node_id': 'originators'}, inplace=True)

        problematicFloods['missing'] = problematicFloods.apply(lambda row: sorted(nodesList - row['received'].union(row['originators'])), axis=1)

        with pd.option_context('display.max_rows', None,'display.max_columns', None, "display.max_colwidth", None,):
            print(problematicFloods[['epoch', 'repetition_i', 'rx_nodes', 'receptions', 'missing']].sort_values('missing').to_csv())

        problematicFloods = problematicFloods[problematicFloods.apply(lambda row: row['missing'] == [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 100, 101, 102, 103, 104, 105, 106, 107, 109, 110, 111, 118, 119, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153], axis=1)]

        plt.hist(problematicFloods['epoch'], bins=100)
        plt.show()

