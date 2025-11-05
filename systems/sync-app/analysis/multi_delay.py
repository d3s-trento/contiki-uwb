#!/usr/bin/env python

import argparse
import subprocess
from io import StringIO

import pandas as pd

from matplotlib import pyplot as plt

import seaborn as sns

parser = argparse.ArgumentParser(
                    prog='multi_delay.py',
                    description='Show the delay of the synchronization flood as a boxplot over multiple experiments')

def parse_name_job_ids(s: str) -> tuple[str, list[int]]:
    ls = s.split(';')

    name = str(ls[0])
    job_ids = list(map(int, ls[1:]))
    
    return name, job_ids

parser.add_argument('jobs', type=parse_name_job_ids, nargs='+')
parser.add_argument('--node', type=int, required=False, default=None)

args = parser.parse_args()

complete_df = pd.DataFrame()

for name, job_ids in args.jobs:
    for job_id in job_ids:
        p = subprocess.Popen(['get_delay.sh', f'{job_id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        if args.node is not None:
            dd = dd[dd['node_id'] == args.node]

        #dd = dd[dd['epoch'].between(dd['epoch'].min() + 30, dd['epoch'].max() - 10)]
        dd = dd[dd['epoch'].between(dd['epoch'].min() + 5, dd['epoch'].max() - 10)]

        dd['delay_us'] = dd['delay_tick'] * 4.0064102564 / 1000

        dd['name'] = name
        dd['job_id'] = job_id

        complete_df = pd.concat([complete_df, dd])

fig,ax = plt.subplots()

xs = complete_df['name'].unique()
boxplot_data = [complete_df[complete_df['name'] == w]['delay_us'] for w in xs]
ax.boxplot(boxplot_data, whis=(0.01, 0.99), showfliers=False)
ax.set_xticklabels(xs)

# sns.boxplot(x='period', y='delay_us',
#             data=complete_df, whis=(0.01, 0.99))

plt.grid()
plt.show()
