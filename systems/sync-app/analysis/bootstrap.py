#!/usr/bin/env python3

import pandas as pd

import sys
from functools import reduce
import argparse

import subprocess
from io import StringIO

def semicolon_separated_int_list(a: str):
    return list(a.split(';'))

parser = argparse.ArgumentParser(
                    prog='reliability.py',
                    description='Calculate the reliability of a bootstrap flood and the following glossy floods')

parser.add_argument('job_ids', type=semicolon_separated_int_list, nargs='+')
parser.add_argument('--summary', action='store_true')

args = parser.parse_args()

def print_missing(df):
    missing_nodes = reduce(lambda x,y: x.union(y), df['missing'], set())
    missing_nodes = sorted(missing_nodes)

    print(' '*8, end='')

    for node in missing_nodes:
        print(f' {node:^5} ', end='')

    print('')

    prevEpoch = -1000

    for epoch, row in df.iterrows():
        print(f'{epoch:7}', end=' ')
        
        if prevEpoch + 1 != epoch:
            print('')

        prevEpoch = epoch

        for node in missing_nodes:
            res = '|' if node in row['missing'] else ' '

            print(f' {res:^5} ', end='')

        print('')

def print_stats(a: dict, csv=False):
    print(f'\tPeriod {a["period"]}')
    print(f'\tEpochs {a["n_epochs"]}')
    print(f'\tNodes {a["n_nodes"]}')
    print(f'\tTotal (epoch, node_id) {a["total_epoch_node"]}')
    print(f'\tBootstraps missed {a["total_epoch_node"]-a["bootstrap_received"]} ({100*(a["total_epoch_node"]-a["bootstrap_received"])/a["total_epoch_node"]:.6}%)')
    print(f'\tBootstraps received {a["bootstrap_received"]} ({100*a["bootstrap_received"]/a["total_epoch_node"]:.6}%)')
    print(f'\t\tOf which {a["bootstrap_scan_received"]} are scans ({100*a["bootstrap_scan_received"]/a["total_epoch_node"]:.6}% of total, {100*a["bootstrap_scan_received"]/a["bootstrap_received"]:.6}% of scans)')

SINK_ID = 119


if args.summary:
    summary_df = pd.DataFrame({'t_sleep': [],
                               'drift_estimation': [],
                               'epochs': [],
                               'nodes': [],
                               'total_attempts': [],
                               'bootstraps_received': [],
                               'received_of_which_scans': []})

for job_ids in args.job_ids:
    period = float(job_ids[0])
    drift_est = str(job_ids[1])
    job_ids = map(int, job_ids[2:])

    result_boostrap = subprocess.Popen(['get_bootstrap.sh', *map(str, job_ids)], stdout=subprocess.PIPE)

    df = pd.read_csv(StringIO(result_boostrap.communicate()[0].decode('utf-8')))
    del result_boostrap

    res = []

    for i, data in df.groupby('job_id'):
        if not args.summary:
            print('\n\n')
            print(f'Job id: {i}')
        data = data[(data['epoch'] > 200) & (data['epoch'] < data['epoch'].max()-5)]
        #data = data[(data['epoch'] > 80) & (data['epoch'] < data['epoch'].max()-5)]

        nodes = set(data['node_id'].unique())

        min_epoch = data['epoch'].min()
        max_epoch = data['epoch'].max()

        if min_epoch != int(min_epoch):
            print('\nERR: min_epoch is not an integer')
            sys.exit(1)

        if max_epoch != int(max_epoch):
            print('\nERR: max_epoch is not an integer')
            sys.exit(1)

        min_epoch = int(min_epoch)
        max_epoch = int(max_epoch)

        data = data[data['node_id'] != SINK_ID]
        #data = data[data['node_id'] != 130]

        nodes = set(data['node_id'].unique())

        #print(nodes)
        #print(len(nodes))

        total_epoch_node = (max_epoch + 1 - min_epoch) * data['node_id'].nunique()
        bootstrap_received = len(data)
        bootstrap_scan_received = len(data[data['scan'] == 1])

        stats = {
            'period': period,
            'drift_est': drift_est,
            'n_epochs': (max_epoch + 1 - min_epoch),
            'n_nodes': data['node_id'].nunique(),
            'total_epoch_node': total_epoch_node,
            'bootstrap_received': bootstrap_received,
            'bootstrap_scan_received': bootstrap_scan_received,
        }
        if not args.summary:
            print_stats(stats)

        res.append(dict(job_id=i, **stats))

        data = data.groupby('epoch').agg({'node_id': set}).reset_index()
        data = data.merge(pd.DataFrame({'epoch': list(range(min_epoch, max_epoch+1))}), on='epoch', how='right')
        data = data.rename(columns={'node_id': 'present'})

        def ww(x):
            if (type(x) is not set):
                x = set([])
                
            return nodes - x

        data['missing'] = list(map(ww, data['present']))

        del data['present']

        if not args.summary:
            tmp = data[list(map(lambda x: len(x) != 0, data['missing']))]
            if len(tmp) != 0:
                print(tmp)

        data = data[data['missing'] != set()]
        data['missing'] = list(map(list, data['missing']))
        data = data.explode('missing')


    if len(set(r['n_nodes'] for r in res)) > 1:
        print('ERR: All the experiments should have the same number of nodes')
        print(res)
        sys.exit(1)

    if len(set(r['drift_est'] for r in res)) > 1:
        print('ERR: All the experiments should have the same drift estimation mode')
        print(res)
        sys.exit(1)

    summary_stats = {
        'period': period,
        'drift_est': res[0]['drift_est'],
        'n_epochs': sum(r['n_epochs'] for r in res),
        'n_nodes': res[0]['n_nodes'],
        'total_epoch_node' : sum(r['total_epoch_node'] for r in res),
        'bootstrap_received' : sum(r['bootstrap_received'] for r in res),
        'bootstrap_scan_received' : sum(r['bootstrap_scan_received'] for r in res),
    }

    if not args.summary:
        print('\n\n')
        print('Summary:')
        print_stats(summary_stats)
    else:
        
        summary_df.loc[len(summary_df)] = [summary_stats["period"],
                                           summary_stats["drift_est"],
                                           summary_stats["n_epochs"],
                                           summary_stats["n_nodes"],
                                           summary_stats["total_epoch_node"],
                                           summary_stats["bootstrap_received"],
                                           summary_stats["bootstrap_scan_received"]]


if args.summary:
    summary_df['bootstraps_missed'] = summary_df['total_attempts'] - summary_df['bootstraps_received']

    summary_df['missed_percent'] = summary_df['bootstraps_missed'] / summary_df['total_attempts'] * 100
    summary_df['received_percent'] = summary_df['bootstraps_received'] / summary_df['total_attempts'] * 100
    summary_df['of_which_scans_percent'] = summary_df['received_of_which_scans'] / summary_df['bootstraps_received'] * 100
    summary_df['of_which_scans_percent_overall'] = summary_df['received_of_which_scans'] / summary_df['total_attempts'] * 100

    summary_df.to_csv('summary.csv')

    print(summary_df)

    tmpData = summary_df\
            .pivot(index='t_sleep', columns=['drift_estimation'], values=['epochs', 'received_percent', 'of_which_scans_percent_overall'])\
            .reset_index()

    print(tmpData.columns)

    formatChosen = {}

    for t in summary_df['drift_estimation'].unique():
        formatChosen[('epochs', t)]                         = "{:0.0f}".format
        formatChosen[('received_percent', t)]               = "{:0.4f}".format
        formatChosen[('of_which_scans_percent_overall', t)] = "{:0.4f}".format

    formatChosen[('t_sleep','')]                            = "{:0.1f}".format

    print(formatChosen)
    tmpData = tmpData.to_latex(formatters=formatChosen)
            
    print(tmpData)
