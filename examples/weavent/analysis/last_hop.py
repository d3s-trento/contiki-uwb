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

import itertools

import gc

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

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--repetitions', dest='repetitions', default=100, help='Set number of repetitions')
parser.add_argument('--percentiles', dest='percentiles', default=[], type=float, nargs='+', help='Value for which to look the percentile')

parser.add_argument('--nodes', dest='nodes', nargs='+', default=None, type=int, help='Nodes to consider in the calculation of the reliability')

parser.add_argument('-hst', '--hop-stability-threshold', dest='hst', default=0.8, type=float, help='Threshold after which we filter out nodes as non-stable')

parser.add_argument('--no-rel', dest='rel', default=False, action='store_true', help='Put results in relation to the base reliability')
parser.add_argument('--rel', dest='rel', default=False, action='store_true', help='Put results in relation to the base reliability')

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

pu.add_plot_arguments(parser)

args = parser.parse_args()

print(args)

P = [0.5, 0.90, 0.99, 0.999, 0.9999, 0.99999]

data = dict()

for l, jobs in args.logs:
    dds = []
    fds = []

    gc.collect()

    for job in jobs:
        job: jr.WeaventLogId

        with job.access(args.force, rx_guard=args.guard, num_nodes=args.numNodes) as a:
            dd = a.time_event_data
            tx_fs = a.tx_fs

            boot = a.bootstrap_data
            tsm = a.tsm_slots

            epochList = set(dd['epoch']).union(set(tx_fs['epoch'])).union(set(boot['epoch'])).union(set(tsm['epoch']))

            epochList = set(range(min(epochList), max(epochList)+1))

            del tsm

            epochList = pd.Series(list(epochList)).to_frame(name='epoch')

            epochList = fu.apply_epoch_filter(args, epochList)

            epochList = set(epochList['epoch'])

            dd = dd[dd['epoch'].isin(epochList)]
            tx_fs = tx_fs[tx_fs['epoch'].isin(epochList)]

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

            if args.nodes is None:
                nodes_hop = boot[['node_id', 'hop']].groupby('node_id')['hop'].agg(pd.Series.mode).reset_index()

                tmp = boot[['node_id', 'hop']].merge(nodes_hop, on='node_id')
                tmp['same'] = tmp['hop_x'] == tmp['hop_y']

                nodes_hop = nodes_hop.merge(tmp.groupby('node_id').mean()['same'].reset_index(), on='node_id')

                nodes_hop = nodes_hop[nodes_hop['same'] >= args.hst]
                nodes_hop = nodes_hop[nodes_hop['hop'] == nodes_hop['hop'].max()]

                args.nodes = set(nodes_hop['node_id'])

            print('Nodes considered {}'.format(args.nodes))

            epochs_to_remove = set(tx_fs[tx_fs['node_id'].isin(set(args.nodes))]['epoch'])

            print('Removing epochs that have as originators {}: {}'.format(args.nodes, epochs_to_remove))

            dd = dd[~dd['epoch'].isin(epochs_to_remove)]
            tx_fs = tx_fs[~tx_fs['epoch'].isin(epochs_to_remove)]

            epochList = epochList - epochs_to_remove

            # Pre-compute reliability
            dd = dd[dd['node_id'].isin(set(args.nodes))]

            ed = dd.groupby(['epoch', 'repetition_i']).size().reset_index()
            tx = tx_fs.groupby(['epoch', 'repetition_i']).size().reset_index()

            #if len(set(tx_fs['node_id']).intersection(set(args.nodes))) > 0:
            #    raise Exception('The --nodes parameter cannot contain any originator')

            del tx_fs

            finalData = pd.merge(ed, tx, on=['epoch', 'repetition_i'], how='outer').fillna(0)

            del tx

            base = pd.DataFrame(itertools.product(epochList, range(0, args.repetitions)), columns=['epoch', 'repetition_i'])
            finalData = pd.merge(finalData, base, how='outer', on=['epoch', 'repetition_i']).fillna(0)

            finalData['rx_nodes'] = len(set(args.nodes))

            finalData['receptions'] = finalData['0_x']

            finalData = finalData[['epoch', 'repetition_i', 'rx_nodes', 'receptions']]

            print('info: Epochs (in job_{}): {}'.format(job.id, finalData.nunique()['epoch']))

            issueMask = (finalData['receptions'] > finalData['rx_nodes'])

            if len(finalData[issueMask]) > 0:
                print('ERROR: Reliability >1 ERROR (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

            issueMask = (finalData['receptions'] < finalData['rx_nodes'])

            if len(finalData[issueMask]) > 0:
                print('Reliability <1 warning (in <job_{}, epoch, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

            finalData = finalData.groupby('repetition_i').sum().reset_index()

            finalData = finalData[['repetition_i', 'rx_nodes', 'receptions']]

            issueMask = (finalData['receptions'] > finalData['rx_nodes'])

            if len(finalData[issueMask]) > 0:
                print('ERROR: Reliability >1 ERROR (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

            issueMask = (finalData['receptions'] < finalData['rx_nodes'])

            if len(finalData[issueMask]) > 0:
                print('Reliability <1 warning (in <job_{}, repetition_i>):\n{}'.format(job.id, finalData[issueMask]))

            if len(finalData['repetition_i'].unique()) != args.repetitions:
                print(f'There are less than {args.repetitions} repetition_i')

            finalData = finalData[['repetition_i', 'receptions', 'rx_nodes']]

            dd['job_id'] = job.id

            dds.append(dd)
            fds.append(finalData)

            gc.collect()

        dd = pd.concat(dds)
        ff_data = pd.concat(fds)

        ff_data = ff_data.groupby(['repetition_i']).sum().reset_index()

        ris = {
            'reliability': {
                'receptionPerc': ff_data['receptions'].sum()/ff_data['rx_nodes'].sum(),
                'receptions': ff_data['receptions'].sum(),
                'rxNodes': ff_data['rx_nodes'].sum(),
            },
            'latencies': []
        }

        baseRel = ris['reliability']['receptionPerc']

        d = dd['diff']

        for p in P:
            if not args.rel:
                ris['latencies'].append((p, np.percentile(d, 100 * p)))
            elif baseRel > p:
                ris['latencies'].append((p, np.percentile(d, 100 * (p/baseRel))))

        data[l] = ris

import pprint
pprint.pprint(data)
