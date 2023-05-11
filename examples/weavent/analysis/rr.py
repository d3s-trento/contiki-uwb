
import pandas as pd
import matplotlib.pyplot as plt
import subprocess
import os

import argparse

import common as c

COMPLETE_FLOOD_NODE_COUNT = 33

parser = argparse.ArgumentParser(
        description='Calculate the reliability of the event flood when there is already a network-wide successful sync flood'
)

parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('log', type=int, nargs='+',
                    help='the id of the log to use')

args = parser.parse_args()

print(args)

for LOG in args.log:

    subprocess.Popen([os.path.join(os.path.dirname(os.path.realpath(__file__)), 'analysis.sh'), f"{LOG}"]).wait()

    data = c.get_event_data(int(LOG))
    epochData = c.get_synch_data(int(LOG))
    #data = pd.read_csv(f'{LOG}.csv')
    #epochData = pd.read_csv(f'e{LOG}.csv')

    if LOG == 5094:
      data = data[data['epoch'] <= 3509]
      epochData = epochData[epochData['epoch'] <= 3509]

    dd = data

    if args.epochFilter:
        dd = c.filter_by_epoch(dd, 10, -10)

    if args.synchFilter:
        dd = c.filter_by_complete_synch(dd, epochData, COMPLETE_FLOOD_NODE_COUNT)
        #w = epochData.groupby('epoch').nunique('node_id') == COMPLETE_FLOOD_NODE_COUNT
        #complete_flood_epochs = set(w.index[w['node_id']])

        #dd = data[data['epoch'].isin(complete_flood_epochs)]

    fig, (ax1, ax2) = plt.subplots(1,2)

    relByEpoch = dd.groupby('epoch').nunique()['node_id']/COMPLETE_FLOOD_NODE_COUNT

    ax1.plot(relByEpoch.reset_index()['epoch'], relByEpoch.reset_index()['node_id'], marker='x', markersize=3, linewidth=0.25)
    ax1.set_title('Reliability by epoch')
    ax1.set_xlabel('epoch')
    ax1.set_ylabel('reliability')

    ax1.grid(which='major', alpha=0.5)
    ax1.grid(which='minor', alpha=0.2)

    relByNode = c.get_rel_by_node(dd)
    N_EPOCHS = dd.nunique()['epoch'].max()

    ax2.plot(relByNode.reset_index()['node_id'], relByNode.reset_index()['epoch'], marker='x', markersize=3, linewidth=0.25)
    ax2.set_title('Reliability by node_id')
    ax2.set_xlabel('node_id')
    ax2.set_ylabel('reliability')

    ax2.grid(which='major', alpha=0.5)
    ax2.grid(which='minor', alpha=0.2)

    ax2.set_xticks(list(range(1,37)), minor=True)

    minEpochs = relByNode.reset_index()['epoch'].min()
    maxEpochs = relByNode.reset_index()['epoch'].max()

    ax1.get_figure().suptitle(LOG)

    plt.savefig(os.path.join(c.CACHE_ANALYSIS_PATH, f'{LOG}/rel.pdf'))

    print(f'{minEpochs}/{maxEpochs} = {minEpochs/maxEpochs}')
plt.show()
