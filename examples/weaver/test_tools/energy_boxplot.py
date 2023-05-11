#!/usr/bin/env python3

import argparse

parser = argparse.ArgumentParser(
        description='Show how the energy consumption is distributed'
)

parser.add_argument('csvs', type=str, nargs='+',
                    help='the paths to the csvs to use')

parser.add_argument('-n', '--separateNodes', dest='nodeSep', default=False, action='store_true', help='Separate the data of the different nodes')
parser.add_argument('-s', '--startEpoch', dest='startEpoch', default=20, type=int, help='Set filter to analyze the data only after epoch x')
parser.add_argument('-e', '--endEpoch', dest='endEpoch', default=None, type=int, help='Set filter to analyze the data only before epoch x')

args = parser.parse_args()

print(args)

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import re

data = [pd.read_csv(p) for p in args.csvs]

for d, p in zip(data, args.csvs):
    label = p

    m = re.match(r"(.*?/)?weaver_s(?P<sink>[0-9]+)o(?P<norg>[0-9]+)w(?P<fs>o?)FS_duration(?P<duration>[0-9]+)/(?P<note>.*)job_(?P<job_id>[0-9]+)/stats/energy.csv", p)

    if m:
        sink, norg, fs, duration, job_id, note = (m.group('sink'), m.group('norg'), m.group('fs'), m.group('duration'), m.group('job_id'), m.group('note'))

        label = f'w/{fs} FS\ns{sink}o{norg}\n{duration}s\njob_{job_id}'

        if m.group('note'):
            label += '\nNote: {}'.format(m.group('note'))

    d['name'] = label

    #d['name'] = p if m == None \
    #            else ('Sink {sink}\nnorg {norg}\nw/{fs} FS\n{duration}s\njob_{job_id}'.format(sink=m.group('sink'),\
    #                                                                  norg=m.group('norg'),\
    #                                                                  fs=m.group('FS'),\
    #                                                                  duration=m.group('duration'),\
    #                                                                  job_id=m.group('job_id')) + ('' if m.group('note') else '\nNote: {}'.format(m.group('note'))))

data = pd.concat(data)

fig,ax = plt.subplots(1,1)
ax.set_title('Energy consumption per epoch and node')

if not args.nodeSep:
    w = [data[data['name'] == n]['e_total'] for n in data['name'].unique()]
else:
    w = [data[(data['name'] == n) & (data['node'] == nid)]['e_total'] for n in data['name'].unique() for nid in data['node'].unique()]

ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
p = ax.violinplot(w, showextrema=False)

if not args.nodeSep:
    ax.set_xticklabels(data['name'].unique())
else:
    ax.set_xticklabels([f'{n}\n{nid}' for n in data['name'].unique() for nid in data['node'].unique()])
#ax.set_ylim([args.minTime, args.maxTime])

plt.show()
