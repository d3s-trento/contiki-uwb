#!/usr/bin/env python3
import slot_viz as sv

import re
import pandas as pd

import argparse
import logging

import jobref
import tsm


def convert_tsm_log(tsm_string, nid, epoch, macroslot, mismatch_slot=[]):
    """It is assumed that the node will sync just once
    per epoch. Therefore there is just one instance of "_".
    """
    tmp = tsm_string

    # TODO: I still need to understand how to deal with mismatches (m)
    # In case there are some in the string, simply do not process the string further
    match = re.match(".*?m", tmp)
    if match:
        tmp = tmp[:match.end()-1]
        logging.warn(f"Found mismatch in tsm string of node {nid} at epoch {epoch}. Processing is limited...")

    match = re.match(".*?_(?P<first_slot>\d+)(_(?P<diff_slot>\d+))?(Y|T)", tmp)
    if not match:
        # this can happen if we fail to bootstrap
        #raise ValueError("Couldn't find a sync slot")
        logging.error(f"Node {nid}, epoch {epoch}: Couldn't find a sync slot. TSM log {tsm_string}")
        return


    slot_idx = int(match.group('first_slot'))

    if match.group('diff_slot'):
        slot_idx += int(match.group('diff_slot'))

    tmp = tmp[match.end() - 1:] # keep every slot after syncing occurred

    while len(tmp) > 0:

        match_y = re.match("^_(\d+)Y", tmp)
        match_d = re.match("^_(\d+)", tmp)
        match_p = re.match("^p(\d+)", tmp)
        match_m = re.match("^m((?:\+|-)\d+)", tmp)

        if match_y:
            slot_idx = int(match_y.group(1))

            tmp = tmp[match_y.end()-1:]
            continue

        if match_d:
            slot_idx += int(match_d.group(1))

            tmp = tmp[match_d.end():]
            continue

        elif match_p:
            # increase slot counter with the progress slot reported.
            slot_idx += int(match_p.group(1)) - 1
            tmp = tmp[match_p.end():]
            continue

        elif match_m:
            slot_idx += int(match_m.group(1)) - 1
            mismatch_slot.append(slot_idx)
            tmp = tmp[match_m.end():]
            continue

        c, tmp = tmp[0], tmp[1:]
        if c in "TRY": # TX, RX AND Y (i.e. first success RX) are already tracked in clogs!
            yield [nid, epoch, slot_idx, c]

        elif c in "LEB#":
            yield [nid, epoch, slot_idx, c]

        elif c in "~D!X":
            for i in range(macroslot-1, -1, -1):
                yield [nid, epoch, slot_idx-i, c]

        elif c in "$":
            yield [nid, epoch, slot_idx, "O"] # Overflow!

        else:
            raise ValueError(f"Unmatched element {c} at {len(tsm_string) - len(tmp)} in {tsm_string}")

        slot_idx += 1


def plot_epoch(tsm_slots_pd, nodes, minSlot=None, maxSlot=None, savedir="."):
    """Given the filepath to cslots.csv file, plot the activity
    of an epoch.

    NOTE:
    -----
    nodes variable is required to display those node that are in
    the simulation, but due to some misbheaviour their info is missing
    for the entire epoch. Pretty paranoic eh?
    """

    # if len(tsm_slots_pd['epoch'].unique()) != 1:
    #     print('There should beexactly one epoch in the passed datagram')

    # # integrate tsm info
    # tsm_slots_pd = tsm_slots_pd[tsm_slots_pd['epoch'] == epoch]

    # align column order to that of cslots
    tsm_slots_pd = tsm_slots_pd[["node_id", "epoch", "slot_idx", "status"]]

    tsm_slots_pd.sort_values(["epoch", "node_id", "slot_idx"], inplace=True)

    # fill in gaps in slots - now there is an entry for each node on each slot
    tsm_slots_pd.sort_values(["slot_idx", "node_id"], inplace=True)

    if minSlot is None:
        minSlot = 0

    if maxSlot is None and tsm_slots_pd['slot_idx'].max() > minSlot + 200:
        logging.warning('Limited max slot_idx to {}. You can use the parameter --maxSlot to specify another limit'.format(minSlot + 200))
        maxSlot = minSlot + 200

    if minSlot is not None:
        tsm_slots_pd = tsm_slots_pd[tsm_slots_pd['slot_idx'] >= minSlot]

    if maxSlot is not None:
        tsm_slots_pd = tsm_slots_pd[tsm_slots_pd['slot_idx'] < maxSlot]

    sv.plot_slots(tsm_slots_pd, nodes, savedir=savedir)


parser = argparse.ArgumentParser()
parser.add_argument("job", type=lambda x: jobref.from_job_log(x, 'tsm_slots.csv'), help="The log file to parse")
parser.add_argument("-e",  "--epoch",     required=True,             type=int,     help="The epoch to plot")
parser.add_argument("-ms", "--macroslot", required=False, default=2, type=int,     help="The size (in slots) of a macroslot")
parser.add_argument("--minSlot",          required=False, default=None, type=int,  help="Maximum slot idx to show (included)")
parser.add_argument("--maxSlot",          required=False, default=None, type=int,  help="Maximum slot idx to show (excluded)")
parser.add_argument('--nodes',            required=False, default=None, type=int, nargs='+', help='Nodes to show')

args = parser.parse_args()

tsm_slots = tsm.get_tsm_slots(args.job, force=False)  # TODO: TMP
tsm_logs = []

if args.nodes is None:
    args.nodes = tsm_slots[tsm_slots['epoch'] == args.epoch]['node_id'].unique()

# Iterate over the tsm logs of that epoch to create the needed data
for index, row in tsm_slots[tsm_slots['epoch'] == args.epoch].iterrows():
    nid, epoch, slots_str = (row['node_id'], row['epoch'], row['slots'])
    epoch = int(epoch)
    nid = int(nid)

    for entry in convert_tsm_log(slots_str, nid, epoch, mismatch_slot=[], macroslot=args.macroslot):
        tsm_logs.append(entry)


tsm_logs_pd  = pd.DataFrame(tsm_logs, columns=["node_id", "epoch", "slot_idx", "status"]).sort_values(["epoch", "node_id", "slot_idx"])

print(tsm_logs_pd[(tsm_logs_pd['slot_idx'].isin(tsm_logs_pd[tsm_logs_pd['status'].isin(['~','D','!'])]['slot_idx'].unique() - args.macroslot)) & (tsm_logs_pd['status'] != 'T')])

plot_epoch(tsm_logs_pd, nodes=args.nodes, minSlot=args.minSlot, maxSlot=args.maxSlot)
