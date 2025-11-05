#!/usr/bin/env python3
import argparse

import pandas as pd

import filtering_utilities as fu
import subprocess
from io import StringIO

import json
import sys

parser = argparse.ArgumentParser(
        description='Calculate the reliability of crystal given a number of originators'
)

# TODO: Needed log in the application to do this
#parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
#parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

fu.add_epoch_filter(parser, True, +35, -10)

parser.add_argument('--numOriginators', dest='numOriginators', required=True, type=int, help='The number of originators')

parser.add_argument('--json',    dest='json', action='store_true',  help='Output as json', default=False)
parser.add_argument('--no-json', dest='json', action='store_false', help='Normal output' , default=False)

parser.add_argument('job_id', type=int, help='the id of the log to use')

args = parser.parse_args()

print(args, file=sys.stderr)

p = subprocess.Popen(['get_rcvd.sh', f'{args.job_id}'], stdout=subprocess.PIPE)
sio = StringIO(p.communicate()[0].decode('utf-8'))
data = pd.read_csv(sio)

p = subprocess.Popen(['get_orig.sh', f'{args.job_id}'], stdout=subprocess.PIPE)
sio = StringIO(p.communicate()[0].decode('utf-8'))
orig_data = pd.read_csv(sio)

data = fu.apply_epoch_filter(args, data)

data.drop_duplicates(['node_id', 'epoch', 'src', 'seqn'], inplace=True)

w = data.groupby('epoch').size().reset_index().rename(columns={0: 'n_rcvd'})
m = orig_data.groupby('epoch').size().reset_index().rename(columns={0: 'n_orig'})

# Make sure the data includes all the epochs
base = pd.DataFrame(list(range(data['epoch'].min(), data['epoch'].max())), columns=['epoch'])

w = pd.merge(w, base, how='outer', on='epoch').fillna(0)
w = pd.merge(m, w, how='outer', on='epoch').fillna(0)

w = w[w['epoch'].between(data['epoch'].min(), data['epoch'].max())]

new_n_orig = w[w['n_orig'] == args.numOriginators]['n_orig'].sum()
new_n_rcvd = w[w['n_orig'] == args.numOriginators]['n_rcvd'].sum()

w = w[w['n_rcvd'] != args.numOriginators]

if len(w) > 0:
    print(f'Data was lost in the following epochs\n{w}', file=sys.stderr)

n_epochs = (data['epoch'].max() - data['epoch'].min() + 1)
n_rcvd = len(data)
n_originated = (n_epochs*args.numOriginators)

reliability = 100*n_rcvd/n_originated
true_rel = 100*new_n_rcvd/new_n_orig
if args.json:
    print(json.dumps({"n_epochs": int(n_epochs), "reliability": float(reliability),  "true_rel": float(true_rel)}))
else:
    print(f'N epochs: {n_epochs}')
    print(f'Reliability {100*n_rcvd/n_originated}% {n_rcvd}/{n_originated}') # TODO: Not great as I could lose an entire epoch at beginning or end and in this case it would not count
    print(f'True rel(removing n_orig != {args.numOriginators})  {100*new_n_rcvd/new_n_orig}% {new_n_rcvd}/{new_n_orig}')
