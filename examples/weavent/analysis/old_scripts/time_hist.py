#!/usr/bin/env python3

import argparse

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

import common as c

parser = argparse.ArgumentParser(
        description='Show how the latency is distributed'
)

parser.add_argument('log', type=int, nargs='+',
                    help='the id of the log to use')

parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--no-cumulative', dest='cumulative', default=False, action='store_false', help='Show the density or the cumulative distribution')
parser.add_argument('--cumulative',    dest='cumulative', default=False, action='store_true',  help='Show the density or the cumulative distribution')

parser.add_argument('--guard', dest='guard', default=0, type=float, help='The guard to consider')

args = parser.parse_args()

dd = []

for i in args.log:
    data = c.compute_time_diff(c.get_event_data(i), rx_guard=args.guard)
    epochData = c.get_synch_data(i)

    if args.epochFilter:
        data = c.filter_by_epoch(data, 10, -10)

    if args.synchFilter:
        data = c.filter_by_complete_synch(data, epochData, 34)

    data['job_id'] = i

    dd.append(data)

dd = pd.concat(dd)

fig, ax = plt.subplots(1, 1)
ax.set_title('Distribution of max latency inside an (epoch, repetition_i)')

d = dd.groupby(['job_id', 'epoch', 'repetition_i'])['diff'].max()

ax.hist(d, bins=100, cumulative=args.cumulative)

for i in range(1, 6):
    p = 1 - 10**-i
    label = f'{p*100}'
    w = label.find('0')
    if w > 3:
        label = label[:w]

    ax.axvline(x=np.percentile(d, p*100), color='red', linestyle='--', label=f'{p*100}'[:i+3] + 'p')
    ax.annotate(p, textcoords='data', rotation=90, xy=(np.percentile(d, p*100), ax.get_ylim()[1]/2))

ax.legend()

plt.show()
