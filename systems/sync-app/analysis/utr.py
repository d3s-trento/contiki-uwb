#!/usr/bin/env python

import os
import sys
import itertools
import argparse
import subprocess
from io import StringIO

import pandas as pd

from matplotlib import pyplot as plt

import seaborn as sns

parser = argparse.ArgumentParser(
                    prog='utr.py',
                    description='Show the utr with which each node receives the syncrhonization flood')

parser.add_argument('job_id', type=int)
parser.add_argument('--node', type=int, nargs='*', required=False, default=[])

args = parser.parse_args()

print(args)

p = subprocess.Popen(['get_utr.sh', f'{args.job_id}'], stdout=subprocess.PIPE)

sio = StringIO(p.communicate()[0].decode('utf-8'))

dd = pd.read_csv(sio)

if len(args.node) > 0:
    dd = dd[dd['node_id'].isin(args.node)]

#dd = dd[dd['epoch'].between(dd['epoch'].min() + 30, dd['epoch'].max() - 10)]
dd = dd[dd['epoch'].between(dd['epoch'].min() + 5, dd['epoch'].max() - 10)]

dd['value'] = dd['value'] / (1 << 17)

print(dd.describe())

sns.scatterplot(x='epoch', y='value', hue='node_id', 
                   data=dd,
                   palette='viridis', s=5)

plt.grid()

plt.show()
