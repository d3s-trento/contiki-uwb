#!/usr/bin/env python3

import pandas as pd

import itertools

import numpy as np

import argparse

import glossy_jobref as jr

import filtering_utilities as fu

import gc


def label_id_pair(a: str) -> tuple[str, int]:
    label, job_ids = a.split(';', 1)

    return (label, [jr.GlossyLogId(int(jid)) for jid in job_ids.split(';')])

parser = argparse.ArgumentParser(
        description='Calculate the reliability of the event flood when there is already a network-wide successful sync flood'
)

fu.add_epoch_filter(parser, False, 10, -10)
fu.add_synch_filter(parser, True, None)
fu.add_completeEpoch_filter(parser, True, None)

parser.add_argument('--no-rel', dest='rel', default=False, action='store_true', help='Put results in relation to the base reliability')
parser.add_argument('--rel', dest='rel', default=False, action='store_true', help='Put results in relation to the base reliability')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--nodes', dest='nodes', nargs='+', default=None, type=int, help='Nodes to consider in the calculation of the reliability')

parser.add_argument('-hst', '--hop-stability-threshold', dest='hst', default=0.8, type=float, help='Threshold after which we filter out nodes as non-stable')

parser.add_argument('logs', nargs='+', type=label_id_pair, help='List of logs')

args = parser.parse_args()

print(args)

P = [0.5, 0.90, 0.99, 0.999, 0.9999, 0.99999]

data = dict()

for l, jobs in args.logs:
    dds = []
    fds = []

    gc.collect()

    for job in jobs:
        job: jr.GlossyLogId

        dds = []
        fds = []

        with job.access(force=args.force, num_nodes=args.numNodes) as a:
            dd = a.latency
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

            if args.completeEpochFilter:
                complete_epochs = set(a.completed_epochs['epoch'])

                removed = epochList - complete_epochs

                if len(removed) > 0:
                    print('Filtering out due not completing the epoch ({}):\n{}'.format(job.id, removed))

                dd = dd[dd['epoch'].isin(complete_epochs)]
                tx_fs = tx_fs[tx_fs['epoch'].isin(complete_epochs)]

                epochList = epochList.intersection(complete_epochs)

            if args.synchFilter:
                complete_bootstrap = set(a.complete_bootstrap['epoch'])

                removed = epochList - complete_bootstrap

                if len(removed) > 0:
                    print('Filtering out due incomplete bootstrap ({}):\n{}'.format(job.id, removed))

                dd = dd[dd['epoch'].isin(complete_bootstrap)]
                tx_fs = tx_fs[tx_fs['epoch'].isin(complete_bootstrap)]

                epochList = epochList.intersection(complete_bootstrap)

            if args.nodes is None:
                nodes_hop = boot[['node_id', 'hop']].groupby('node_id')['hop'].agg(pd.Series.mode).reset_index()

                tmp = boot[['node_id', 'hop']].merge(nodes_hop, on='node_id')
                tmp['same'] = tmp['hop_x'] == tmp['hop_y']

                nodes_hop = nodes_hop.merge(tmp.groupby('node_id').mean()['same'].reset_index(), on='node_id')

                nodes_hop = nodes_hop[nodes_hop['same'] >= args.hst]
                nodes_hop = nodes_hop[nodes_hop['hop'] == nodes_hop['hop'].max()]

                args.nodes = set(nodes_hop['node_id'])

            print('Nodes considered {}'.format(args.nodes))


            dd = dd[dd['node_id'].isin(set(args.nodes))]

            ed = dd.groupby(['epoch', 'repetition_i']).size().reset_index()
            tx = tx_fs.groupby(['epoch', 'repetition_i']).size().reset_index()

            if len(set(tx_fs['node_id']).intersection(set(args.nodes))) > 0:
                raise Exception('The --nodes parameter cannot contain any originator')

            del tx_fs

            finalData = pd.merge(ed, tx, on=['epoch', 'repetition_i'], how='outer').fillna(0)

            del tx

            base = pd.DataFrame(itertools.product(epochList, range(0, 50)), columns=['epoch', 'repetition_i'])
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

            if len(finalData['repetition_i'].unique()) != 50:
                print('There are less than 50 repetition_i')

            finalData = finalData[['repetition_i', 'receptions', 'rx_nodes']]

            dd['job_id'] = job.id
            finalData['job_id'] = job.id

            dd['duration'] /= 1000

            dds.append(dd)
            fds.append(finalData)

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

        d = dd['duration']

        for p in P:
            if not args.rel:
                ris['latencies'].append((p, np.percentile(d, 100 * p)))
            elif baseRel > p:
                ris['latencies'].append((p, np.percentile(d, 100 * (p/baseRel))))

        data[l] = ris


import pprint
pprint.pprint(data)
