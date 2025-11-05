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
                    prog='delay.py',
                    description='Show the delay with which each node receives the syncrhonization flood')

parser.add_argument('job_id', type=int)
parser.add_argument('--node', type=int, required=False, default=None)

args = parser.parse_args()

p = subprocess.Popen(['get_delay.sh', f'{args.job_id}'], stdout=subprocess.PIPE)

sio = StringIO(p.communicate()[0].decode('utf-8'))

dd = pd.read_csv(sio)

if args.node is not None:
    dd = dd[dd['node_id'] == args.node]

#dd = dd[dd['epoch'].between(dd['epoch'].min() + 30, dd['epoch'].max() - 10)]
dd = dd[dd['epoch'].between(dd['epoch'].min() + 5, dd['epoch'].max() - 10)]

#dd = dd[dd['node_id'] != 130]

dd['delay_us'] = dd['delay_tick'] * 4.0064102564 / 1000

print(dd.describe())

sns.scatterplot(x='epoch', y='delay_us', hue='node_id', 
                   data=dd,
                   palette='viridis', s=5)

plt.grid()

plt.show()
