#!/usr/bin/env python3

import common as c

import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.ticker as mtick
import subprocess
import os

import argparse

COMPLETE_FLOOD_NODE_COUNT = 33

parser = argparse.ArgumentParser(
        description='Calculate the reliability of the event flood when there is already a network-wide successful sync flood'
)

parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--numNodes', dest='numNodes', type=int, help='The number of nodes onto which to calculate the result')

parser.add_argument('log', type=int, nargs='+',
                    help='the ids of the log to use')

args = parser.parse_args()

print(args)

total_epochs = 0
eventData = []
tx_fs_data = []

for id in args.log:

    epochData = c.get_synch_data(id)
    dd = c.get_event_data(id)

    tx_fs = c.get_tx_fs_data(id)

    tsm = c.get_tsm_slots(id).groupby('epoch').nunique().reset_index()
    epochList = set(tsm[tsm['node_id'] == args.numNodes + 1]['epoch'])

    # First filter the epochs so that all nodes completed the epoch
    epochData = epochData[epochData['epoch'].isin(epochList)]
    dd = dd[dd['epoch'].isin(epochList)]
    tx_fs = tx_fs[tx_fs['epoch'].isin(epochList)]

    # TODO: These filters do not work properly

    if args.epochFilter:
        minEpoch = min(epochList) + 10
        maxEpoch = max(epochList) - 10
        dd = c.filter_by_epoch(dd, minEpoch, maxEpoch)
        tx_fs = c.filter_by_epoch(tx_fs, minEpoch, maxEpoch)
        epochList = {v for v in epochList if v > minEpoch and v < maxEpoch}

        print(dd['epoch'].min(), tx_fs['epoch'].min(), dd['epoch'].max(), tx_fs['epoch'].max())

    if args.synchFilter:
        # TODO: Check if this includes the sink or not
        dd = c.filter_by_complete_synch(dd, epochData, args.numNodes+1)
        tx_fs = c.filter_by_complete_synch(tx_fs, epochData, args.numNodes+1)

    eventData.append(dd)
    tx_fs_data.append(tx_fs)
    total_epochs += len(epochList)

print(f'total_epochs: {total_epochs}')

eventData = pd.concat(eventData)
tx_fs_data = pd.concat(tx_fs_data)

finalData = pd.merge(eventData.groupby('repetition_i').size().reset_index(), tx_fs_data.groupby('repetition_i').size().reset_index(), on='repetition_i', how='outer')
finalData = finalData.fillna(0)
finalData['reliability'] = (finalData['0_x'] + finalData['0_y'])/(total_epochs*(args.numNodes + 1))


#finalData = ((eventData.groupby('repetition_i').size() + tx_fs_data.groupby('repetition_i').size())/(total_epochs*(args.numNodes + 1))).reset_index()
finalData['reliability'] *= 100;
ax = finalData.plot(x='repetition_i', y='reliability')
ax.yaxis.set_major_formatter(mtick.PercentFormatter())
plt.show()

print('{}% {}/{}'.format(finalData['reliability'].sum()/100, (finalData['0_x'] + finalData['0_y']).sum(), len( (finalData['0_x'] + finalData['0_y']) )*total_epochs*(args.numNodes + 1)))
