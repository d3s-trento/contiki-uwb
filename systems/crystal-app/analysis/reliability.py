#!/usr/bin/env python3
import argparse

import pandas as pd

import filtering_utilities as fu

parser = argparse.ArgumentParser(
        description='Calculate the reliability of crystal given a number of originators'
)

# TODO: Needed log in the application to do this
#parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
#parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

fu.add_epoch_filter(parser, True, +35, -10)

parser.add_argument('--numOriginators', dest='numOriginators', required=True, type=int, help='The number of originators')

parser.add_argument('log', type=str, help='the id of the log to use')  #TODO: TMP integrate with the rest

args = parser.parse_args()

data = pd.read_csv(args.log)

data = fu.apply_epoch_filter(args, data)

print(data)

data.drop_duplicates(['node_id', 'epoch', 'src', 'seqn'], inplace=True)

w = data.groupby('epoch').size().reset_index()

# Make sure the data includes all the epochs
base = pd.DataFrame(list(range(data['epoch'].min() + 10, data['epoch'].max() - 10)), columns=['epoch'])
w = pd.merge(w, base, how='outer').fillna(0)

w = w[w[0] != args.numOriginators].rename(columns={0: 'n_rcvd'})

if len(w) > 0:
    print(f'Data was lost in the following epochs\n{w}')

n_rcvd = len(data)
n_originated = ((data['epoch'].max() - data['epoch'].min() + 1)*args.numOriginators)

# TODO: Not great as I could lose an entire epoch at beginning or end and in this case it would not count
print(f'Reliability {100*n_rcvd/n_originated}% {n_rcvd}/{n_originated}')
