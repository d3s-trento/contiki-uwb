#!/usr/bin/env python

import os
import sys
import itertools
import argparse
import subprocess
from io import StringIO

import pandas as pd

class Fraction:
    def __init__(self, numerator, denominator):
        self.numerator = numerator
        self.denominator = denominator

    def __str__(self):
        return f'{100*self.numerator/self.denominator:8.4f}% ({self.numerator}/{self.denominator})'

parser = argparse.ArgumentParser(
                    prog='reliability.py',
                    description='Calculate the reliability of a bootstrap flood and the following glossy floods')

parser.add_argument('job_id', type=int, nargs='+')

parser.add_argument('--sinkId', type=int, required=False, default=119)

args = parser.parse_args()

result_boostrap = subprocess.Popen(['get_bootstrap.sh', *map(str, args.job_id)], stdout=subprocess.PIPE)

all_bootstrap = pd.read_csv(StringIO(result_boostrap.communicate()[0].decode('utf-8')))
del result_boostrap

result_glossy = subprocess.Popen(['get_latency.sh', *map(str, args.job_id)], stdout=subprocess.PIPE)

all_data      = pd.read_csv(StringIO(result_glossy.communicate()[0].decode('utf-8')))
del result_glossy

for job_id in set(all_bootstrap['job_id'].unique()).union(set(all_data['job_id'].unique())):
    print(f'\n\n{job_id}')
    bootstrap = all_bootstrap[all_bootstrap['job_id'] == job_id]
    data = all_data[all_data['job_id'] == job_id]

    min_epoch = max(min(bootstrap['epoch'].min(), data['epoch'].min()), 200)
    max_epoch = max(bootstrap['epoch'].max(), data['epoch'].max()) - 5

    bootstrap = bootstrap[bootstrap['epoch'].between(min_epoch, max_epoch)]
    data      = data[data['epoch'].between(min_epoch, max_epoch)]

    # Remove the sink
    bootstrap = bootstrap[bootstrap['node_id'] != args.sinkId]
    data = data[data['node_id'] != args.sinkId]
    #bootstrap = bootstrap[bootstrap['node_id'] != 30]
    #data = data[data['node_id'] != 30]

    nodes = set(bootstrap['node_id'].unique()).union(set(data['node_id'].unique()))

    n_nodes = len(nodes)
    print(f'Assuming {n_nodes} nodes are used')

    bootstrap_by_node_id = bootstrap.copy()

    def see_missing(df):
        df_tmp = df.copy()
        return set(list(range(df_tmp['epoch'].min(), df_tmp['epoch'].max() +1))) - set(df_tmp['epoch'])

    for n in nodes:
        missing = sorted(list(see_missing(bootstrap[bootstrap['node_id'] == n])))
        if len(missing) != 0:
            print(f'{n}:')
            for m in itertools.batched(missing, n=5):
                print(m)
            print('')

    bootstrap = bootstrap.groupby('epoch').nunique()['node_id'].reset_index()

    valid_epochs = bootstrap[bootstrap['node_id'] == (n_nodes)]
    del valid_epochs['node_id']

    bootstrap_by_node_id = bootstrap_by_node_id.groupby('node_id')['epoch'].nunique()

    ep, nd = zip(*itertools.product(valid_epochs['epoch'], nodes-set([args.sinkId])))
    to_merge = pd.DataFrame({'epoch': ep, 'node_id': nd})

    data_by_node_id = data.groupby(['epoch', 'node_id']).nunique()['repetition_i'].reset_index()
    data_by_node_id = data_by_node_id.merge(to_merge, how='right', on=['epoch', 'node_id'])

    print('Number of epochs used: {} ({}-{}) with {} of epochs with all nodes receiving the bootstrap'.format(max_epoch-min_epoch+1, min_epoch, max_epoch, len(valid_epochs)))
    print('Reliability of the bootstrap {}'.format(Fraction((bootstrap['node_id']-1).sum(), (max_epoch-min_epoch+1)*(n_nodes-1) ) ))
    with pd.option_context('display.max_rows', None):
        j = 1

        for i, v in bootstrap_by_node_id.items():
            v = Fraction(v, max_epoch-min_epoch+1)
            print(f'{i:4}: {v}', end='\n' if j == 0 else '\t')
            j = (j + 1) % 6

    print('')

    print('Reliability of the syncrhonized glossy floods {}'.format( Fraction(data_by_node_id.groupby('node_id').sum()['repetition_i'].sum(), ((len(valid_epochs))*10*(n_nodes-1))) ))
    with pd.option_context('display.max_rows', None):
        j = 1

        for i, v in data_by_node_id.groupby('node_id').sum()['repetition_i'].items():
            v = Fraction(v, 10*len(valid_epochs))
            print(f'{i:4}: {v}', end='\n' if j == 0 else '\t')
            j = (j + 1) % 6

    print('')
