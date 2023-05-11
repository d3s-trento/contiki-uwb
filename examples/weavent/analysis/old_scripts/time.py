#!/bin/python

import sys
import argparse

import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import os

import common as c

parser = argparse.ArgumentParser()
for i in [s.split('=')[0][2:] for s in sys.argv]:
    if i.isdigit():
        parser.add_argument(f'--{i}', type=str)

parser.add_argument('--no-sameEpochs', dest='sameEpochs', default=True, action='store_false', help='filter by only common epochs')
parser.add_argument('--sameEpochs',    dest='sameEpochs', default=True, action='store_true',  help='filter by only common epochs')

parser.add_argument('--no-slotCompensation', dest='slotCompensation', default=False, action='store_false', help='Compensate that some nodes might start before the slot')
parser.add_argument('--slotCompensation',    dest='slotCompensation', default=False, action='store_true',  help='Compensate that some nodes might start before the slot')

parser.add_argument('--min-time', dest='minTime', default=None, type=float, help='min to show in the graph')
parser.add_argument('--max-time', dest='maxTime', default=None, type=float, help='min to show in the graph')

parser.add_argument('--guard', dest='guard', default=0.1, type=float, help='the rx_guard used')

args = parser.parse_args()

ids = [(int(k), v) for k, v in args.__dict__.items() if k.isdigit()]

# ids = [
#     (5079,'not modified'),
#     (5076,'w/o setting txbuf'),
#     (5080,'w/o setting txbuf and forcetrxoff'),
#     (5081,'w/o setting txbuf and forcetrxoff and set tx_fctrl just before rxenable'),
#     (5082,'w/o setting txbuf and forcetrxoff and set tx_fctrl just before rxenable'),
#     (5085,'plen128 w/o setting txbuf and forcetrxoff and set tx_fctrl just before rxenable'),
#     (5093,'optimized plen128 w/o setting txbuf and forcetrxoff and set tx_fctrl just before rxenable'),
#
#     (5089,'plen128 w/o setting txbuf and forcetrxoff and set tx_fctrl just before rxenable'),
#     (5102,'plen64 custom isr'),
#     (5105,'plen64 more custom isr')
#     (5106,'plen64 more custom isr'),
#     (5138,'plen64 more custom isr and write buf')
#
#     (5167, 'later 1'),
#     (5168, 'later 2'),
#     (5169, 'later 3'),
#
#     (5176, 'later 1'),
#     (5177, 'later 2'),
#     (5178, 'later 3'),
#     (5180, 'later 4'),
# ]

data = dict()
epochData = dict()

for i, _ in ids:
    data[i] = c.compute_time_diff(c.get_event_data(i), rx_guard=args.guard)
    epochData[i] = c.get_synch_data(i)

if args.sameEpochs:
    MAX = min(data[i]['epoch'].max()-50 for i,_ in ids)
    MIN = 10
else:
    MAX = -10
    MIN = 10

for i, _ in ids:
  data[i] = c.filter_by_epoch(data[i], MIN, MAX)
  #data[i] = c.filter_by_complete_synch(data[i], epochData[i], 33)

print(data)

ids = [(i, f'{l}\n(job_{i})') for i, l in ids]

d, ll = [], []

sorted(ids, key=lambda x: x[1])

for i, l in ids:
    ll = [*ll, *([l]*len(data[i].groupby(['epoch', 'repetition_i'])['diff'])) ]

    if args.slotCompensation:
        d = [*d, *(data[i].groupby(['epoch', 'repetition_i'])['diff'].max() - data[i].groupby(['epoch', 'repetition_i']).min()['diff'].apply(lambda x: min(0,x)))]
    else:
        d = [*d, *data[i].groupby(['epoch', 'repetition_i'])['diff'].max()]

df = pd.DataFrame(dict(d=d, l=ll))

#ax = df.boxplot(by='l', sym='+')

w = [df[df.l == l]['d'].values for _, l in ids]

fig, ax = plt.subplots()
ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
p = ax.violinplot(w, showextrema=False)

ax.set_xticks([y+1 for y in range(len(ids))])
ax.set_xticklabels([l for i,l in ids])

ax.annotate('sink 19 w/o nodes 21,22 (5-6 hops)',
            xy=(5, 5), xycoords='figure pixels')

ax.set_ylabel('max distance per epoch (ms)')
ax.set_xlabel('modality')
ax.get_figure().suptitle('Duration of fast propagating flood')

ax.set_ylim([args.minTime, args.maxTime])

plt.show()

print(df.groupby('l').describe())
