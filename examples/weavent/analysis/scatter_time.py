#!/bin/python

import argparse

parser = argparse.ArgumentParser(
        description='Show how the latency is distributed wrt the nodes'
)

parser.add_argument('log', type=int, nargs='+', help='the id of the log to use')

parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('--min-time', dest='minTime', default=None, type=float, help='min to show in the graph')
parser.add_argument('--max-time', dest='maxTime', default=None, type=float, help='min to show in the graph')

parser.add_argument('--guard', dest='guard', default=0.0, type=float, help='the rx_guard used')

args = parser.parse_args()

COMPLETE_FLOOD_NODE_COUNT = 33

import matplotlib.pyplot as plt
import common as c

for LOG in args.log:
    data = c.get_event_data(int(LOG))
    epochData = c.get_synch_data(int(LOG))
    eventData = c.get_event_sent_data(int(LOG))

    if args.epochFilter:
        data = c.filter_by_epoch(data, 10, -10)
        eventData = c.filter_by_epoch(eventData, 10, -10)

    if args.synchFilter:
        data = c.filter_by_complete_synch(data, epochData, COMPLETE_FLOOD_NODE_COUNT)
        eventData = c.filter_by_complete_synch(eventData, epochData, COMPLETE_FLOOD_NODE_COUNT)

    data = c.compute_time_diff(data, rx_guard=args.guard)

    fig,ax = plt.subplots(1,1)
    ax.set_title(LOG)

    data = data.sort_values('node_id')

    w = [data[data['node_id'] == nid]['diff'] for nid in data['node_id'].unique()]

    k = data.groupby('node_id').describe()['diff']
    print(f'LOG: {LOG}')
    print(k)

    ax.boxplot(w, sym='x', flierprops=dict(markersize=3, alpha=0.7))
    p = ax.violinplot(w, showextrema=False)

    ax.set_xticklabels(data['node_id'].unique())
    ax.set_ylim([args.minTime, args.maxTime])

plt.show()
