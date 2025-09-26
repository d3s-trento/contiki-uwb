#!/usr/bin/env python3

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick

import itertools

import numpy as np

import argparse

import glossy_jobref as jr

import plot_utility as pu
import filtering_utilities as fu

import gc


def from_job(a: str):
    return jr.GlossyLogId(int(a))

parser = argparse.ArgumentParser(
        description='Calculate the reliability of the event flood when there is already a network-wide successful sync flood'
)

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--max-nslots', dest='nslots', type=float, default=1e6, help='Maximum slot to consider')
parser.add_argument('--slotDuration', dest='duration', type=float, default=471.79487, help='Maximum slot to consider')

pu.add_plot_arguments(parser)

parser.add_argument('log', type=from_job, nargs='+', help='the ids of the log to use')

args = parser.parse_args()

print(args)

ff_data = []

for job in args.log:
    job: jr.GlossyLogId

    gc.collect()

    with job.access(force=args.force, num_nodes=args.numNodes) as a:
        dd = a.latency
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

        if args.synchFilter:
            complete_bootstrap = set(a.complete_bootstrap['epoch'])

            removed = epochList - complete_bootstrap

            print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, sorted(removed)))

            dd = dd[dd['epoch'].isin(complete_bootstrap)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(complete_bootstrap)]

            epochList = epochList.intersection(complete_bootstrap)

        if args.completeEpochFilter:
            complete_epochs = set(a.completed_epochs['epoch'])

            removed = epochList - complete_epochs

            print('Filtering out due not completing the epoch ({}):\n{}'.format(job.id, sorted(removed)))

            dd = dd[dd['epoch'].isin(complete_epochs)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(complete_epochs)]

            epochList = epochList.intersection(complete_epochs)

        dd = dd[dd['duration'] < args.nslots*args.duration*1000]

        ed = dd.groupby(['epoch', 'repetition_i']).size().reset_index()

        del dd

        tx = tx_fs.groupby(['epoch', 'repetition_i']).size().reset_index()

        del tx_fs

        finalData = pd.merge(ed, tx, on=['epoch', 'repetition_i'], how='outer').fillna(0)

        del tx

        base = pd.DataFrame(itertools.product(epochList, range(0, 50)), columns=['epoch', 'repetition_i'])
        finalData = pd.merge(finalData, base, how='outer', on=['epoch', 'repetition_i']).fillna(0)

        finalData['rx_nodes'] = a.num_nodes + 1 - finalData['0_y']
        finalData['total_nodes'] = a.num_nodes + 1

        finalData['receptions'] = finalData['0_x']
        finalData['positives'] = finalData['0_x'] + finalData['0_y']

        finalData = finalData[['epoch', 'repetition_i', 'rx_nodes', 'total_nodes', 'receptions', 'positives']]

        print('info: Epochs (in job_{}): {}'.format(job.id, finalData.nunique()['epoch']))

        issueMask = (finalData['receptions'] > finalData['rx_nodes']) | (finalData['positives'] > finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('ERROR: Reliability >1 ERROR (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        issueMask = (finalData['receptions'] < finalData['rx_nodes']) | (finalData['positives'] < finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('Reliability <1 warning (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        finalData = finalData.groupby('repetition_i').sum().reset_index()

        finalData = finalData[['repetition_i', 'rx_nodes', 'total_nodes', 'receptions', 'positives']]

        issueMask = (finalData['receptions'] > finalData['rx_nodes']) | (finalData['positives'] > finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('ERROR: Reliability >1 ERROR (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        issueMask = (finalData['receptions'] < finalData['rx_nodes']) | (finalData['positives'] < finalData['total_nodes'])

        if len(finalData[issueMask]) > 0:
            print('Reliability <1 warning (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

        if len(finalData['repetition_i'].unique()) != 50:
            print('There are less than 50 repetition_i')

        finalData = finalData[['repetition_i', 'receptions', 'positives', 'rx_nodes', 'total_nodes']]

    ff_data.append(finalData)

ff_data = pd.concat(ff_data)
ff_data = ff_data.groupby(['repetition_i']).sum().reset_index()

print('reception reliability: {} {}/{}'.format(ff_data['receptions'].sum()/ff_data['rx_nodes'].sum(), ff_data['receptions'].sum(), ff_data['rx_nodes'].sum()))
print('total reliability: {} {}/{}'.format(ff_data['positives'].sum()/ff_data['total_nodes'].sum(), ff_data['positives'].sum(), ff_data['total_nodes'].sum()))

ff_data['p_reception_reliability'] = (ff_data['receptions'] / ff_data['rx_nodes']) * 10
ff_data['p_total_reliability'] = (ff_data['positives'] / ff_data['total_nodes']) *10
