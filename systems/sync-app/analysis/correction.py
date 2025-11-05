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
                    prog='correction.py',
                    description='Show the correction applied at each epoch by each node')

parser.add_argument('job_id', type=int)
parser.add_argument('--node', type=int, required=False, default=None)

parser.add_argument('--xlim', type=float, nargs=2, required=False, default=(None,None))
parser.add_argument('--ylim', type=float, nargs=2, required=False, default=(None,None))


args = parser.parse_args()
print(args)


p = subprocess.Popen(['get_correction.sh', f'{args.job_id}'], stdout=subprocess.PIPE)

sio = StringIO(p.communicate()[0].decode('utf-8'))

dd = pd.read_csv(sio)

if args.node is not None:
    dd = dd[dd['node_id'] == args.node]

print(dd)

#dd = dd[dd['epoch'].between(dd['epoch'].min() + 30, dd['epoch'].max() - 10)]
dd['correction_us'] = dd['correction_ns'] / 1000

print(dd.describe())

fig,ax = plt.subplots(1)

sns.scatterplot(x='epoch', y='correction_us', hue='node_id', 
                data=dd,
                palette='viridis', s=5, ax=ax)

ax.set_xlim(args.xlim)
ax.set_ylim(args.ylim)


plt.show()
