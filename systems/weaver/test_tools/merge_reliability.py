#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(
        description='Show how the energy consumption is distributed'
)

parser.add_argument('dir', type=str,
                    help='Folder containing all the tests to consider')

parser.add_argument('-n', '--separateNodes', dest='nodeSep', default=False, action='store_true', help='Separate the data of the different nodes')
parser.add_argument('-pg', '--plotGain', dest='plotGain', default=False, action='store_true', help='Plot the energy consumption gain obtained')
parser.add_argument('-pr', '--plotReliability', dest='plotReliability', default=False, action='store_true', help='Plot the reliability obtained')

args = parser.parse_args()

print(args)

path = args.dir

import json
import os
import pandas as pd
import re
import matplotlib.pyplot as plt
import matplotlib as mpl
import matplotlib.ticker as mtick

rel_data = []

for cls in [p for p in os.listdir(path) if p.startswith('weaver_s')]:
    jobs_path = [os.path.join(path, cls, f) for f in os.listdir(os.path.join(path, cls))]
    jobs_path = [f for f in jobs_path if os.path.isdir(f) and os.path.basename(f).startswith('job_')]

    m = re.match(r"weaver_s(?P<sink>[0-9]+)o(?P<norg>[0-9]+)w(?P<fs>o?)FS_duration(?P<duration>[0-9]+)", cls)

    if not m:
        print('ERROR: Can\'t decode class name')

    sink, norg, fs, duration = (m.group('sink'), m.group('norg'), m.group('fs'), m.group('duration'))

    cls_data = {'name': cls, 'sink': sink, 'norg': norg, 'fs': fs != 'o', 'duration': duration}

    rel_data.extend([{
        **cls_data,
        'job_id': j,
        **json.load(open(os.path.join(j, 'stats', 'summary.json'), 'r')),
        'avg_energy': pd.read_csv(os.path.join(j, 'stats', 'energy.csv'))['e_total'].describe()['mean'],
    } for j in jobs_path])

rel_data = pd.json_normalize(rel_data)
rel_data['a_sink'] = rel_data['a_rate_sink']*rel_data['n_epochs']
rel_data['e_total'] = rel_data['avg_energy']*rel_data['n_epochs']

print(rel_data)
aggr = rel_data.groupby(['name', 'sink', 'norg', 'fs', 'duration']).agg({'a_sink': 'sum', 'n_epochs': 'sum', 'e_total': 'sum'}).reset_index()
aggr['norg'] = aggr['norg'].astype('int')

aggr['a_sink_rate'] = aggr['a_sink']/aggr['n_epochs']
aggr['e_avg'] = aggr['e_total']/aggr['n_epochs']

final = aggr.pivot(index='norg', columns=['fs'], values=['a_sink_rate', 'e_avg', 'n_epochs'])
final['gain'] = 1 - final['e_avg'][True]/final['e_avg'][False]

# NOTE: For now we ignore duration
print(final)

if args.plotGain:
    fig,ax = plt.subplots(1,1)
    ax.set_title('Gain in energy consumption wrt the number of originators')

    ff = final.reset_index()

    ax.plot(ff['norg'], ff['gain']*100, 'x-')
    ax.axhline(y=0, color='black', linestyle='-', linewidth=mpl.rcParams['axes.linewidth'])

    ax.yaxis.set_major_formatter(mtick.PercentFormatter())

    ax.set_ylabel('Gain (in %)')
    ax.set_xlabel('Number of originators')

    plt.show()


if args.plotReliability:
    fig,ax = plt.subplots(1,1)
    ax.set_title('Reliability')

    ff = final.reset_index()

    ax.plot(ff['norg'], ff['a_sink_rate'][True]*100, 'x-', label='w/ FS')
    ax.plot(ff['norg'], ff['a_sink_rate'][False]*100, 'x-', label='w/o FS')

    ax.axhline(y=0, color='black', linestyle='-', linewidth=mpl.rcParams['axes.linewidth'])
    ax.axhline(y=100, color='black', linestyle='-', linewidth=mpl.rcParams['axes.linewidth'])

    ax.yaxis.set_major_formatter(mtick.PercentFormatter())

    ax.set_ylabel('Reliability')
    ax.set_xlabel('Number of originators')

    ax.legend()

    plt.show()

