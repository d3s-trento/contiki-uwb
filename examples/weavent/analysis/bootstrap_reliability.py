#!/usr/bin/env python3

import pandas as pd

import argparse

import common as c
import weavent_jobref as jr
import filtering_utilities as fu

def from_job(a: str):
    return jr.WeaventLogId(int(a))


parser = argparse.ArgumentParser(
        description='Calculate the reliability of the bootstrap flood'
)

parser.add_argument('--numNodes', dest='numNodes', default=None, type=int, help='The number of nodes onto which to calculate the result')

parser.add_argument('--force', dest='force', default=False, action='store_true', help='Force recomputation')

parser.add_argument('--nodes', dest='nodes', default=None, nargs='+', type=int, help='Filter the nodes you are interested in. If empty nothing is filtered')

parser.add_argument('log', type=from_job, nargs='+', help='the ids of the log to use')

fu.add_epoch_filter(parser, False, 10, -10)

args = parser.parse_args()

print(args)

data = []

for job in args.log:
    job: jr.WeaventLogId

    with job.access(force=args.force, num_nodes=args.numNodes) as a:
        dd = a.bootstrap_data

        (dd,) = fu.apply_epoch_filter(args, dd)

        if args.nodes is None:
            tmp = dd[dd['hop'] != 0]
            numNodes = a.num_nodes
        else:
            tmp = dd[(dd['hop'] != 0) & (dd['node_id'].isin(set(args.nodes)))]
            numNodes = len(set(args.nodes))

        bootstraps = tmp.groupby('epoch').size().reset_index().rename(columns={0: 'receptions'})

        base = pd.DataFrame(range(min(dd['epoch']), max(dd['epoch'])+1), columns=['epoch'])
        dd = pd.merge(bootstraps, base, how='outer', on=['epoch']).fillna(0)

        dd['job_id'] = job.id

    data.append(dd)

data = pd.concat(data)

lostBootstraps = data[data['receptions'] < numNodes]
if len(lostBootstraps) > 0:
    print('WARNING: Lost bootstraps:\n{}'.format(lostBootstraps))

print('Bootstrap reliability: {} ({}/{})'.format(
    data['receptions'].mean()/(numNodes),
    data['receptions'].sum(),
    len(data)*(numNodes)
))


