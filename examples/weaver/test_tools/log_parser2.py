#!/usr/bin/env python3
import os
import csv, json
import logging
import re

import copy as cp
from povo_parser import match_filter

from functools import reduce
from pathlib import Path
from collections import OrderedDict
from itertools import product

import numpy as np
import pandas as pd

import matplotlib
import matplotlib.pyplot as plt

from slot_viz import plot_slots, serialize_slots
from plot_decorators import *

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.DEBUG)
logging.getLogger(__name__).setLevel(level=logging.DEBUG)

mpl_logger = logging.getLogger(matplotlib.__name__)
mpl_logger.setLevel(logging.ERROR)

pd.set_option('display.max_colwidth', None)

BITMAP_ORDER = r"^BO\s+\d+,\s+(?P<node_ids>(?:\d+\s+)*)"
NODE_BITMAP     = r"^ME\s+\d+,\s+(?P<node_ids>(?:\d+\s+)*)"
ACK             = r"^ACK\s+(?P<epoch>\d+),\s+(?P<node_ids>(?:\d+\s+)*)"
CLOG            = r"(?i)^E\s+(?P<epoch>\d+),\s+I\s+(?P<slot_idx>\d+),\s+L\s+(?P<status>[trleby#]),\s+D\s+(?P<distance>\d+),\s+S\s+(?P<sender>\d+),\s+H\s+(?P<lhs>\d+),\s+A\s+(?P<acked>0x[0-9a-fA-F]+),\s+B\s+(?P<buffer>0x[0-9a-fA-F]+)$"
TSM_LOG         = r"(?i)^\[tsm\s+(?P<epoch>\d+)\]Slots:\s+(?P<slots_desc>[_trleby#+\-0-9mp$]+)$"
IS_SINK         = r"(?i)^is_sink$"
IS_ORIG         = r"(?i)^E\s+(?P<epoch>\d+),\s+is_orig$"
NSLOTS          = r"(?i)^E\s+(?P<epoch>\d+),\s+NSLOTS\s+(?P<nslots>\d+)$"
OUT             = r"(?i)^OUT\s+(?P<epoch>\d+),\s+O\s+(?P<sender>\d+),\s+S\s+(?P<seqno>\d+)$"
IN              = r"(?i)^IN\s+(?P<epoch>\d+),\s+O\s+(?P<sender>\d+),\s+S\s+(?P<seqno>\d+),\s+I\s+(?P<slot_idx>\d+)$"
STATETIME       = r"(?i)^STATETIME\s+E\s+(?P<epoch>\d+),\s+I\s+(?P<idle>\d+),\s+TP\s+(?P<tx_preamble>\d+),\s+TD\s+(?P<tx_data>\d+),\s+RH\s+(?P<rx_hunting>\d+),\s+RP\s+(?P<rx_preamble>\d+),\s+RD\s+(?P<rx_data>\d+)"
RSTATS          = r"(?i)^E\s+(?P<epoch>\d+),\s+TX\s+(?P<ntx>\d+),\s+RX\s+(?P<nrx>\d+),\s+TO\s+(?P<nto>\d+),\s+ER\s+(?P<nerr>\d+)$"
BOOT            = r"BOOT\s+(?P<epoch>\d+),\s+B\s+(?P<booted>\d+),\s+N\s+(?P<nmisses>\d+),\s+L\s+(?P<leaked>\d+)"

LABEL_IS_SINK = "sink"
LABEL_IS_ORIG = "orig"
LABEL_BITMAP_ORDER = "bo"
LABEL_NODE_BITMAP  = "me"
LABEL_ACK          = "ack"
LABEL_CLOG         = "crystalloid_log"
LABEL_TSM_LOG      = "tsm_log"
LABEL_NSLOTS       = "nslots"
LABEL_STATETIME    = "statetime"
LABEL_OUT = "out"
LABEL_IN  = "in"
LABEL_RSTATS = "rstats"
LABEL_BOOT= "boot"
LABEL_OTHER = "other"

# define rules with the corresponding label.
# be careful to define longer rules first!
FILTER_RULES = OrderedDict([
    (re.compile(CLOG),              LABEL_CLOG),
    (re.compile(TSM_LOG),           LABEL_TSM_LOG),
    (re.compile(ACK),               LABEL_ACK),
    (re.compile(NODE_BITMAP),       LABEL_NODE_BITMAP),
    (re.compile(BITMAP_ORDER),      LABEL_BITMAP_ORDER),
    (re.compile(NSLOTS),            LABEL_NSLOTS),
    (re.compile(IS_SINK),           LABEL_IS_SINK),
    (re.compile(OUT),               LABEL_OUT),
    (re.compile(IN),                LABEL_IN),
    (re.compile(RSTATS),            LABEL_RSTATS),
    (re.compile(IS_ORIG),           LABEL_IS_ORIG),
    (re.compile(BOOT),              LABEL_BOOT),
    (re.compile(STATETIME),         LABEL_STATETIME),
])

CLOGS_HEADER = ["node", "epoch", "slot_idx", "distance", "status", "sender_id", "lhs", "acked", "buffer", "ack_upd"]

def convert_tsm_log(tsm_string, nid, epoch, mismatch_slot=[]):
    """It is assumed that the node will sync just once
    per epoch. Therefore there is just one instance of "_".
    """
    #assert tsm_string.count("_") == 1
    tmp = tsm_string

    # TODO: I still need to understand how to deal with mismatches (m)
    # In case there are some in the string, simply do not process the string further
    match = re.match(".*?m", tmp)
    if match:
        tmp = tmp[:match.end()-1]
        logger.warn(f"Found mismatch in tsm string of node {nid} at epoch {epoch}. Processing is limited...")

    match = re.match(".*_(?P<first_slot>\d+)", tmp)
    if not match:
        # this can happen if we fail to bootstrap
        #raise ValueError("Couldn't find a sync slot")
        logger.error(f"Node {nid}, epoch {epoch}: Couldn't find a sync slot. TSM log {tsm_string}")
        return


    slot_idx = int(match.group(1))
    tmp = tmp[match.end():] # keep every slot after syncing occurred

    while len(tmp) > 0:

        match_p = re.match("p(\d+)", tmp)
        match_m = re.match("m((?:\+|-)\d+)", tmp)

        if match_p:
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

        elif c in "$":
            yield [nid, epoch, slot_idx, "O"] # Overflow!

        else:
            raise ValueError(f"Unmatched element {c} at {len(tsm_string) - len(tmp)} in {tsm_string}")

        slot_idx += 1

def bitmap_hex2bin_array(bitmap_hex_str, bitmap_len=64):
    bitmap_number = int(bitmap_hex_str, 16)
    return np.array(list(reversed(np.base_repr(bitmap_number).zfill(bitmap_len)))).astype(np.uint8)

def bitmap_to_string(bitmap_array):
    return str.join("", map(str, bitmap_array))

def bitmap_to_nodelist(bitmap_str, nodes_ordering):
    nodes = []
    for i in range(0, min(len(bitmap_str), len(nodes_ordering))):
        if bitmap_str[i] == "1":
            nodes.append(nodes_ordering[i])
        elif bitmap_str[i] == "0":
            pass # just a check
        else:
            raise ValueError(f"Non-binary number found in bitmap {bitmap_str}")
    return nodes

def nodelist_to_bitmap(nodes, nodes_ordering):
    bitmap = [0 for _ in range(64)]
    mapping = {nodes_ordering[i]: i for i in range(0, len(nodes_ordering))}
    for node in nodes:
        bitmap[mapping[node]] = 1
    return bitmap

def bitmap_apply(bmap, bitmap_order):
    if isinstance(bmap, str):
        return bitmap_to_nodelist(bmap, bitmap_order)
    return np.nan


def parse_log(filepath, parser, ignore_slots=True, info={}):
    sinks = set()
    origs = OrderedDict()
    nodes = set()

    bitmap_order = None

    tsm_logs = []
    clogs  = []
    nslots = []
    acks   = []
    errs   = []
    rstats = []
    bootings = []
    mismatches = []
    statetime = []

    last_bitmap = {}

    line = 0
    sent = []
    rcvd = []
    for nid, log in parser(filepath):

        nid = int(nid)
        nodes.add(nid)
        rule, match = match_filter(log, FILTER_RULES, compiled=True)

        if rule == LABEL_IS_SINK:
            sinks.add(nid)

        elif rule == LABEL_IS_ORIG:
            epoch = match.group(1)
            epoch = int(epoch)
            if epoch in origs:
                origs[epoch].append(nid)
            else:
                origs[epoch] = [nid]

        elif rule == LABEL_BITMAP_ORDER:
            order = list(map(int, match.groupdict()["node_ids"].split()))
            if bitmap_order is None:
                bitmap_order = order
            elif order != bitmap_order:
                raise ValueError(f"Two nodes have different deployments: {str(bitmap_order)} vs {str(order)}")

        elif rule == LABEL_NODE_BITMAP:
            nodes_found = list(map(int, match.groupdict()["node_ids"].split()))
            if len(nodes_found) == 0 or nodes_found[0] != nid:
                raise ValueError(f"Node {nid} was not included in the deployment correctly")

        elif rule == LABEL_ACK:
            epoch, acked = match.groups()
            acked = list(map(int, acked.split()))
            epoch = int(epoch)
            acked = bitmap_to_string(nodelist_to_bitmap(acked, bitmap_order))
            acks.append([nid, epoch, acked])

        elif rule == LABEL_NSLOTS:
            epoch, ns = tuple(map(int, match.groups()))
            nslots.append([nid, epoch, ns])

        elif rule == LABEL_CLOG and ignore_slots is False:
            epoch, slot_idx, status, distance, sender_id, lhs, acked, buffer = match.groups()
            epoch, slot_idx, distance, sender_id, lhs = tuple(map(int, (epoch, slot_idx, distance, sender_id, lhs)))

            acked, buffer = tuple(map(bitmap_to_string, map(bitmap_hex2bin_array, (acked, buffer))))

            if nid not in last_bitmap:
                last_bitmap[nid] = None

            entry = [nid, epoch, slot_idx, distance, status, sender_id, lhs, acked, buffer, last_bitmap[nid] != acked]

            last_bitmap[nid] = acked
            clogs.append(entry)

        elif rule == LABEL_TSM_LOG:
            epoch, slots_str = match.groups()
            epoch = int(epoch)

            entry = [nid, epoch, slots_str.lower().count("e")]
            errs.append(entry)

            mmatch = [] # TODO this could be reported in a separate file
            for entry in convert_tsm_log(slots_str, nid, epoch, mismatch_slot=mmatch):
                tsm_logs.append(entry)

            for slot in mmatch:
                mismatches.append([nid, epoch, slot])

        elif rule == LABEL_IN:
            epoch, sender, seqno, slot_idx = tuple(map(int, match.groups()))
            rcvd.append([nid, epoch, sender, seqno, slot_idx])

        elif rule == LABEL_OUT:
            epoch, sender, seqno = tuple(map(int, match.groups()))
            sent.append([nid, epoch, sender, seqno])

        elif rule == LABEL_STATETIME:

            statetime_info = list(map(int, match.groups()))
            statetime.append([nid] +  statetime_info)

        elif rule == LABEL_RSTATS:

            epoch, ntx, nrx, nto, nerr = tuple(map(int, match.groups()))
            rstats.append([nid, epoch, ntx, nrx, nto, nerr])

        elif rule == LABEL_BOOT:
            epoch, booted, nmisses, leaked = tuple(map(int, match.groups()))
            bootings.append([nid, epoch, booted, nmisses, leaked])

        else:
            pass

    origs = [[epoch, bitmap_to_string(nodelist_to_bitmap(originators, bitmap_order))] for epoch, originators in origs.items()]

    # sorting is very important for every dataframe. Later analysis could assume
    # dfs to have been already sorted (!)
    origs_pd = pd.DataFrame(origs, columns=["epoch", "originators"]).sort_values(["epoch"])
    acks_pd = pd.DataFrame(acks, columns=["node", "epoch", "acked"]).sort_values(["epoch", "node"])
    errs_pd = pd.DataFrame(errs, columns=["node", "epoch", "nerr"]).sort_values(["epoch", "node"])
    mismatches_pd = pd.DataFrame(mismatches, columns=["node", "epoch", "slot"]).sort_values(["epoch", "node"])
    nslots_pd = pd.DataFrame(nslots, columns=["node", "epoch", "nslots"]).sort_values(["epoch", "node"])
    tsm_logs_pd  = pd.DataFrame(tsm_logs, columns=["node", "epoch", "slot_idx", "status"]).sort_values(["epoch", "node", "slot_idx"])
    sent_pd = pd.DataFrame(sent, columns=["node", "epoch", "sender_id", "seqno"]).sort_values(["epoch", "node", "sender_id", "seqno"])
    rcvd_pd = pd.DataFrame(rcvd, columns=["node", "epoch", "sender_id", "seqno", "slot_idx"]).sort_values(["epoch", "node", "sender_id", "seqno"])
    clogs_pd  = pd.DataFrame(clogs, columns=CLOGS_HEADER).sort_values(["epoch", "node", "slot_idx"])
    statetime_pd = pd.DataFrame(statetime, columns=["node", "epoch", "idle", "tx_preamble", "tx_data", "rx_hunting", "rx_preamble", "rx_data"])\
        .sort_values(["epoch", "node"])
    rstats_pd = pd.DataFrame(rstats, columns=["node", "epoch", "ntx", "nrx", "nto", "nerr"])\
        .sort_values(["epoch", "node"])
    boot_pd = pd.DataFrame(bootings, columns=["node", "epoch", "booted", "nmisses", "leaked"]).sort_values(["epoch", "node"])

    nodes, sinks = tuple(map(sorted, map(list, [nodes, sinks])))
    epochs = list(map(int, sorted(acks_pd["epoch"].unique()))) # from numpy int64 to python int...(!)
    info = {"nodes": nodes, "sinks": sinks, "epochs": epochs, "bitmap_order": bitmap_order}
    return info, sent_pd, rcvd_pd, clogs_pd, nslots_pd, origs_pd, acks_pd, tsm_logs_pd, errs_pd, mismatches_pd, statetime_pd, rstats_pd, boot_pd

def read_stats(stats_dir):
    with open(str(stats_dir.joinpath("stats.json"))) as json_file:
        stats = json.load(json_file)
    return stats, \
        pd.read_csv(str(stats_dir.joinpath("sent.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("rcvd.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("cslots.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("nslots.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("originators.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("acks.csv"))), \
        pd.read_csv(str(stats_dir.joinpath("statetime_traces.csv"))),\
        pd.read_csv(str(stats_dir.joinpath("rstats.csv"))),\
        pd.read_csv(str(stats_dir.joinpath("boot.csv")))

def get_ack_rate(acks_pd, origs_pd, stats):
    sim_epochs = sorted(origs_pd["epoch"].unique())
    acks_pd = acks_pd[acks_pd["epoch"].isin(sim_epochs)].copy()
    epoch_diff = set(acks_pd.epoch.unique()).symmetric_difference(set(sim_epochs))
    if len(epoch_diff) > 0:
        logger.warn("Some epochs don't have receptions: %s" % epoch_diff)
    if sorted(acks_pd["epoch"].unique()) != sim_epochs:
        raise ValueError("Epoch mismatch between originators and acks, the code doesn't deal with this. Possible log cut?")

    acks_pd["n_acked"] = acks_pd["acked"].apply(lambda x: len(bitmap_apply(x, stats.bitmap_order)))
    # I was not able to avoid this approach... still it's quite fast (atm)
    origs_dict = {row["epoch"]: row for row in origs_pd.to_dict(orient="row")}
    acks_pd["a_rate"] = acks_pd.apply(lambda row: row.n_acked / max(origs_dict[row.epoch]["n_originators"], 1), axis=1)
    return acks_pd

def get_ack_rate2(sent_pd, rcvd_pd, n_orig, sink_id, log=True, savedir=None):
    """Compute sink pdr using sent and received logs.
    This computation perform more checks than get_ack_rate, which
    compute the pdr based on the ACK bitmaps.
    Both function should be used.
    """
    info = lambda x: logger.info(x) if log is True else None

    sent_pd = sent_pd.copy()
    sim_epochs = sorted(sent_pd["epoch"].unique())
    rcvd_pd = rcvd_pd[rcvd_pd["epoch"].isin(sim_epochs)].copy()
    # probably what follows is more correct
    # rcvd_pd = rcvd_pd[(rcvd_pd["epoch"] >= min_epoch) & (rcvd_pd["epoch"] <= max_epoch)].copy()

    # check that the same amount of senders is present at each epoch
    sent_c0 = sent_pd.groupby(["epoch"]).size().reset_index(name="n_senders")
    c0 = sent_c0[sent_c0["n_senders"] != n_orig]
    if len(c0):
        info("[X] Check 0: Same number of senders distributed among epochs")
        if SEVERITY_STRICT is True:
            raise ValueError("Unequal number of senders distributed among epochs:\n %s" % c0.head())
    else:
        info("[V] Check 0: Same number of senders distributed among epochs")

    sent_c1 = sent_pd.groupby(["sender_id", "seqno"]).size().reset_index(name="count")
    c1 = sent_c1[sent_c1["count"] > 1]
    if len(c1):
        raise ValueError("Some packets have been transmitted in multiple epochs (epoch leakage)\n %s" % c1.head())
    info("[V] Check 1: All packets have been propagated in individual epochs.")

    rcvd_c2 = rcvd_pd.groupby(["node", "sender_id", "seqno"]).size().reset_index(name="count")
    c2 = rcvd_c2[rcvd_c2["count"] > 1]
    if len(c2):
        raise ValueError("Some packets have been received more than once.\n %s" % c2.head())
    info("[V] Check 2: All packets received have been delivered only once.")

    epoch_diff = set(rcvd_pd["epoch"].unique()).symmetric_difference(set(sim_epochs))
    if len(epoch_diff) > 0:
        logger.warn("Some epochs don't have receptions: %s" % epoch_diff)

    sent_pd.rename(columns={'epoch': 'epoch_sent'}, inplace=True)
    sent_pd.drop(["node"], axis=1, inplace=True)
    rcvd_pd.rename(columns={'epoch': 'epoch_rcvd'}, inplace=True)
    merge_pd = pd.merge(sent_pd, rcvd_pd, on=["sender_id", "seqno"], how="outer", validate="one_to_one")
    lost_pd = merge_pd[merge_pd["epoch_rcvd"].isna()]
    if len(lost_pd) > 0 and not (savedir is None):
        lost_pd.to_csv(str(savedir.joinpath("pkt_lost.csv")), index=False)

    rcvd_c3 = merge_pd[~merge_pd["epoch_rcvd"].isna()].copy()
    rcvd_c3["epoch_match"] = np.nan
    rcvd_c3["epoch_match"] = rcvd_c3.apply(lambda row: 1 if row["epoch_sent"] == row["epoch_rcvd"] else 0, axis=1)
    c3 = rcvd_c3[rcvd_c3["epoch_match"] == 0]
    if len(c3):
        info("[X] Check 3: All packets have been originated and delivered in the same epoch.")
        c3 = c3.drop("epoch_match", axis=1)
        if not savedir is None:
            c3.to_csv(str(savedir.joinpath("pkt_ooo.csv")), index=False)
        if SEVERITY_STRICT is True:
            raise ValueError("Some packets have been received in a different epoch\n %s" % c3.head())
    else:
        info("[V] Check 3: All packets have been originated and delivered in the same epoch.")

    # remove packet we didn't see they were sent
    merge_pd = merge_pd[~merge_pd["epoch_sent"].isna()].copy()
    # now we know that each packet has been sent once
    sent = merge_pd[["epoch_sent", "sender_id", "seqno"]].drop_duplicates()
    n_pkt = len(sent)
    rcvd_pd = merge_pd[~merge_pd["epoch_rcvd"].isna()].copy()
    del_pd = rcvd_pd.groupby(["node"]).size().reset_index(name="n_delivered")
    del_pd["n_pkt"] = n_pkt
    del_pd["pdr"] = del_pd["n_delivered"] / n_pkt
    if len(del_pd) == 0:
        del_pd = pd.DataFrame([[sink_id, 0, 0, 0]], columns=["node", "n_delivered", "n_pkt", "pdr"])
    return del_pd

def get_rx_slots(rcvd_pd, sink_id):
    rcvd = rcvd_pd[rcvd_pd["node"] == sink_id].copy()
    rcvd.sort_values(["node", "epoch", "slot_idx"], inplace=True)
    # remove possible duplicates (there shouldn't be any from the C code)
    # we don't trigger errors here since they are called in ack_rate2
    prev_len = len(rcvd)
    rcvd.drop_duplicates(["sender_id", "seqno"], inplace=True)
    assert(prev_len == len(rcvd))

    if len(rcvd) == 0:
        return np.nan, np.nan, np.nan

    rcvd = rcvd.groupby(["node", "epoch"]).agg({"slot_idx": ["min", "max", np.mean]}).reset_index()
    rcvd.columns = ["node", "epoch", "slot_first_rx", "slot_last_rx", "slot_avg_rx"]
    rcvd = rcvd.groupby("node").agg({"slot_first_rx": np.mean, "slot_last_rx": np.mean, "slot_avg_rx": np.mean}).reset_index()

    assert len(rcvd) in (0, 1)

    slot_first_rx = rcvd.slot_first_rx.tolist()[0]
    slot_last_rx = rcvd.slot_last_rx.tolist()[0]
    slot_avg_rx  = rcvd.slot_avg_rx.tolist()[0]

    return slot_first_rx, slot_last_rx, slot_avg_rx


@tight_decorator
@boxplot_decorator
def plot_nslots(nslots_pd, n_originators, n_nodes):
    fig = plt.figure(figsize=get_fig_dimensions(1, 1, width=3.25 * max(1, int(n_nodes/4))))
    ax = fig.add_subplot("111")
    nslots_pd.boxplot("nslots", by="node", ax=ax)
    ax.set_xlabel("Nodes")
    ax.set_ylabel("Number of slots before termination")
    ax.set_title(f"{n_originators} concurrent originators")
    fig.suptitle("")
    return fig

@tight_decorator
@boxplot_decorator
def plot_arate(arate_pd, sink_id, n_originators, n_nodes):
    fig = plt.figure(figsize=get_fig_dimensions(1, 1, width=3.25 * max(1, int(n_nodes/4))))
    arate_pd = arate_pd.copy()
    if not len(arate_pd):
        return fig
    sink_arate = arate_pd[arate_pd.node == sink_id]
    sink_miss  = sink_arate[sink_arate.a_rate < 1.00]
    ax = fig.add_subplot("111")
    arate_pd.boxplot("a_rate", by="node", ax=ax)
    ax.set_xlabel("Nodes")
    ax.set_ylabel("% nodes acknowledged")
    ax.set_title(f"{n_originators} concurrent originators")
    xspan = ax.get_xlim()[1] - ax.get_xlim()[0]
    yspan = ax.get_ylim()[1] - ax.get_ylim()[0]
    tx, ty = ax.get_xlim()[0] + xspan * 0.025, ax.get_ylim()[0] + yspan * 0.025
    ax.text(tx, ty, f"N epochs with\nsink A rate < 1:\n{len(sink_miss)} out of {len(sink_arate)}",\
            bbox={"facecolor":"linen", "alpha": 0.5, "edgecolor": "gray"})
    fig.suptitle("")
    return fig

def epoch_indexer(cslots_fp, epoch_column_idx=1, has_header=True):
    def save_index(buffer, epoch, from_row, nrows):
        if epoch in buffer:
            raise ValueError("Attempt to insert a duplicate epoch")
        buffer[epoch] = OrderedDict((("from_row", from_row), ("nrows", nrows)))

    header = []
    indexes = OrderedDict()
    line = 0
    cur_epoch = None
    with open(cslots_fp, "r") as fh:
        csv_reader = csv.reader(fh, delimiter=",")
        for entry in csv_reader:

            if line == 0 and has_header is True:
                line += 1
                header = cp.deepcopy(entry)
                continue

            epoch = int(entry[epoch_column_idx])
            if epoch != cur_epoch:
                if cur_epoch is not None:
                    save_index(indexes, cur_epoch, from_row, line - from_row)

                cur_epoch = epoch
                from_row  = line

            line += 1

        if cur_epoch is not None:
            save_index(indexes, cur_epoch, from_row, None)

    return header, indexes

def epoch_slots_uniform(epoch_pd, nodes):
    """Uniform the shape of actions performed on each slot by
    nodes involed. Filling up NA where no information on the slot
    is available.
    """
    max_slot = epoch_pd["slot_idx"].max()
    other_columns = len(epoch_pd.columns) - 3 # in the next step we set 3 columns only
    epoch = epoch_pd["epoch"].unique() # return an array
    if len(epoch) != 1:
        raise ValueError("Attempt to uniform more than one epoch")
    epoch = epoch[0]

    # define homogeneous shape for each node (with NA entries)
    entries = [[nid, epoch, slot_idx] + [np.nan] * other_columns for nid in nodes for slot_idx in range(0, max_slot + 1)]
    base_pd = pd.DataFrame(entries, columns=cp.deepcopy(epoch_pd.columns)).sort_values(["epoch", "node", "slot_idx"])

    # "merge" base with actual epoch information.
    # concat base after epoch, then drop duplicates keeping only the first entry.
    # This way the information from base is removed if the same entry is already
    # in epoch_pd
    epoch_pd = pd.concat([epoch_pd, base_pd], axis=0, ignore_index=True) # concat on rows
    epoch_pd.drop_duplicates(["node", "epoch", "slot_idx"], keep="first", inplace=True)
    return epoch_pd.sort_values(["epoch", "node", "slot_idx"]).reset_index()

def plot_epoch(cslots_fp, tsm_slots_fp, nodes, bitmap_order, epoch, savedir=".", interactive=False):
    """Given the filepath to cslots.csv file, plot the activity
    of an epoch.

    NOTE:
    -----
    nodes variable is required to display those node that are in
    the simulation, but due to some misbheaviour their info is missing
    for the entire epoch. Pretty paranoic eh?
    """
    # retrieve epochs' rows
    clogs_header, epoch_indexes = epoch_indexer(cslots_fp)
    skiprows, nrows = epoch_indexes[epoch]["from_row"], epoch_indexes[epoch]["nrows"]

    # this will load a single epoch
    epoch_pd = pd.read_csv(cslots_fp, skiprows=skiprows, nrows=nrows, index_col=False, names=clogs_header)

    # integrate tsm info
    tsm_slots_pd = pd.read_csv(tsm_slots_fp)
    tsm_slots_pd = tsm_slots_pd[(tsm_slots_pd.epoch == epoch) & (~tsm_slots_pd.status.isin(["T", "R", "Y"]))]

    # extend dataframe
    tsm_slots_pd["distance"] = np.nan
    tsm_slots_pd["sender_id"] = np.nan
    tsm_slots_pd["lhs"] = np.nan
    tsm_slots_pd["acked"] = np.nan
    tsm_slots_pd["buffer"] = np.nan
    # align column order to that of cslots
    tsm_slots_pd = tsm_slots_pd[["node", "epoch", "slot_idx", "distance", "status", "sender_id", "lhs", "acked", "buffer"]]

    epoch_pd = pd.concat([epoch_pd, tsm_slots_pd], axis=0, ignore_index=True) # concat on rows
    epoch_pd.sort_values(["epoch", "node", "slot_idx"], inplace=True)

    # fill in gaps in slots - now there is an entry for each node on each slot
    epoch_pd = epoch_slots_uniform(epoch_pd, nodes)
    epoch_pd.sort_values(["slot_idx", "node"], inplace=True)

    epoch_pd["acked"]  = epoch_pd["acked"].apply(lambda x: bitmap_apply(x, bitmap_order))
    epoch_pd["buffer"] = epoch_pd["buffer"].apply(lambda x: bitmap_apply(x, bitmap_order))
    plot_slots(epoch_pd, nodes, savedir=savedir, interactive=interactive)
    serialize_slots(epoch_pd, nodes, savedir=savedir)

def summarize(sink_id, sent_pd, rcvd_pd, nslots_pd, origs_pd, acks_pd, stats):
    # obtain ACK rate dataframe for the simulation
    arate_pd = get_ack_rate(acks_pd, origs_pd, stats)

    # create summary dataframe with per-epoch stats;
    # begin with sink ACK rate, i.e., packets received by the sink
    summary_pd = arate_pd.loc[(arate_pd.node == sink_id), ["epoch", "a_rate"]]
    summary_pd.rename(columns={'a_rate': 'a_rate_sink'}, inplace=True)

    # overall ACK rate (indicates the circulation of ACKs in the network)
    mean_arate_pd = arate_pd.groupby(["epoch"])[["a_rate"]].mean().reset_index()
    mean_arate_pd.rename(columns={'a_rate': 'a_rate_mean'}, inplace=True)
    summary_pd = pd.merge(summary_pd, mean_arate_pd, on="epoch", how="left")

    # number of slots for termination of the sink
    sink_nslots_pd = nslots_pd.loc[(nslots_pd.node == sink_id), ["epoch", "nslots"]]
    sink_nslots_pd.rename(columns={'nslots': 'last_slot_sink'}, inplace=True)
    summary_pd = pd.merge(summary_pd, sink_nslots_pd, on="epoch", how="left")

    # number of slots for termination (overall)
    mean_nslots_pd = nslots_pd.groupby(["epoch"]).agg(
        last_slot_avg=("nslots", "mean"), last_slot_max=("nslots", "max")).reset_index()
    summary_pd = pd.merge(summary_pd, mean_nslots_pd, on="epoch", how="left")

    last_cslots_pd = get_last_new_pkt(rcvd_pd, sink_id)
    last_cslots_pd.rename(columns={'slot_idx': 'last_new_pkt_slot_sink'}, inplace=True)
    summary_pd = pd.merge(summary_pd, last_cslots_pd, on="epoch", how="left")

    last_cslots_pd = get_first_new_pkt(rcvd_pd, sink_id)
    last_cslots_pd.rename(columns={'slot_idx': 'first_new_pkt_slot_sink'}, inplace=True)
    summary_pd = pd.merge(summary_pd, last_cslots_pd, on="epoch", how="left")

    # aggregate across all epochs
    summary_mean = summary_pd.mean()
    summary_mean.at["epoch"] = None
    summary_pd = summary_pd.append(summary_mean, ignore_index=True)

    return summary_pd

def get_last_new_pkt(cslots_pd, sink_id):
    # slot number of the last (new) packet received by sink,
    # useful to debug and to fine-tune the termination policy;
    # from cslots_pd:
    # 1. keep only R entries (reception) of the sink, if there is a payload
    # 2. order by slot_idx
    # 3. remove duplicates based on [epoch, node, acked]
    # Note: in 2., use "keep first" to filter previously-received packets
    cslots_mask = (cslots_pd.status == "R") & (cslots_pd.node == sink_id) & (cslots_pd.sender_id != 65535) & (cslots_pd.sender_id != sink_id)
    last_cslots_pd = cslots_pd.loc[cslots_mask, ["epoch", "node", "acked", "slot_idx"]]
    last_cslots_pd = last_cslots_pd.sort_values(["node", "epoch", "slot_idx"], ascending=False)
    last_cslots_pd = last_cslots_pd.drop_duplicates(subset=["epoch", "node"], keep="first")
    last_cslots_pd.drop(["node", "acked"], axis=1, inplace=True)
    return last_cslots_pd

def get_last_new_pkt(rcvd_pd, sink_id):
    # slot number of the last (new) packet received by sink,
    # useful to debug and to fine-tune the termination policy;
    # from cslots_pd:
    # 1. order by slot_idx
    # 2. remove duplicates based on [epoch, node]
    # Note: in 2., use "keep first" to filter previously-received packets
    mask = (rcvd_pd.node == sink_id) & (rcvd_pd.sender_id != 65535) & (rcvd_pd.sender_id != sink_id)
    last_pd = rcvd_pd.loc[mask, ["epoch", "node", "slot_idx"]]
    last_pd = last_pd.sort_values(["node", "epoch", "slot_idx"], ascending=False)
    last_pd = last_pd.drop_duplicates(subset=["epoch", "node"], keep="first")
    last_pd.drop(["node"], axis=1, inplace=True)
    return last_pd

def get_first_new_pkt(rcvd_pd, sink_id):
    # slot number of the last (new) packet received by sink,
    # useful to debug and to fine-tune the termination policy;
    # from cslots_pd:
    # 1. order by slot_idx
    # 2. remove duplicates based on [epoch, node]
    # Note: in 2., use "keep first" to filter previously-received packets
    mask = (rcvd_pd.node == sink_id) & (rcvd_pd.sender_id != 65535) & (rcvd_pd.sender_id != sink_id)
    last_pd = rcvd_pd.loc[mask, ["epoch", "node", "slot_idx"]]
    last_pd = last_pd.sort_values(["node", "epoch", "slot_idx"], ascending=True)
    last_pd = last_pd.drop_duplicates(subset=["epoch", "node"], keep="first")
    last_pd.drop(["node"], axis=1, inplace=True)
    return last_pd

def get_tsm_error(tsm_slots_fp, sink_id):
    tsm_slots_pd = pd.read_csv(tsm_slots_fp)
    slot_mask = (tsm_slots_pd.status == "E") & (tsm_slots_pd.node == sink_id)
    tsm_slots_pd = tsm_slots_pd.loc[slot_mask, ["epoch", "slot_idx"]]
    return tsm_slots_pd

def get_stats_lastrx(sink_id, sim_epochs, cslots_fp, tsm_slots_fp):
    # get empty ack rx and errors before the sink receives
    # the last new pkt
    cslots_pd = pd.read_csv(cslots_fp)
    cslots_mask = (cslots_pd.status == "R") & (cslots_pd.node == sink_id) & ((cslots_pd.sender_id == 65535) | (cslots_pd.sender_id == sink_id))
    last_cslots_pd = cslots_pd.loc[cslots_mask, ["epoch", "node", "slot_idx"]]
    last_cslots_pd = last_cslots_pd.sort_values("slot_idx", ascending=True)
    last_cslots_pd.drop(["node"], axis=1, inplace=True)
    gack_rx = last_cslots_pd

    sink_last_new = get_last_new_pkt(cslots_pd, sink_id)
    sink_last_new.rename(columns={'slot_idx': 'last_slot'}, inplace=True)
    rx_before_last = pd.merge(gack_rx, sink_last_new, on="epoch")
    # filter
    rx_before_last = rx_before_last[rx_before_last.slot_idx < rx_before_last.last_slot]
    # count rx
    rx_before_last.drop(["slot_idx", "last_slot"], axis=1, inplace=True)
    rx_before_last = rx_before_last.groupby("epoch").size().reset_index()
    rx_before_last.columns = ["epoch", "nrx"]

    err_pd = get_tsm_error(tsm_slots_fp, sink_id)
    err_before_last = pd.merge(err_pd, sink_last_new, on="epoch")
    # filter
    err_before_last = err_before_last[err_before_last.slot_idx < err_before_last.last_slot]
    # count errors
    err_before_last.drop(["slot_idx", "last_slot"], axis=1, inplace=True)
    err_before_last = err_before_last.groupby("epoch").size().reset_index()
    err_before_last.columns = ["epoch", "nerr"]

    stat_before_last = pd.merge(rx_before_last, err_before_last, on="epoch")
    stat_before_last = pd.merge(sink_last_new, stat_before_last, on="epoch").sort_values("epoch")
    stat_before_last = stat_before_last[stat_before_last["epoch"].isin(sim_epochs)]
    return stat_before_last

def get_statetime_info(statetime_traces_pd, savedir=Path(".")):
    MAX_T = 4e6
    # remove epoch where an underflow occurred
    traces_pd = statetime_traces_pd.copy()
    mask = (traces_pd.idle > MAX_T) | (traces_pd.rx_hunting > MAX_T) |\
        (traces_pd.tx_preamble > MAX_T) | (traces_pd.tx_data > MAX_T) |\
        (traces_pd.rx_preamble > MAX_T) | (traces_pd.rx_data > MAX_T)
    epochs_to_remove = traces_pd[mask].epoch.unique()

    if len(epochs_to_remove) > 0:
        logger.warn("Found statetime underflows, removing the following epochs from energy computation: %s" % traces_pd[mask].head())

    traces_pd = traces_pd[~traces_pd.epoch.isin(epochs_to_remove)]

    # Configuration Considered: Ch 2, plen 128, 64Mhz PRF, 6.8Mbps Datarate
    # Setting N of page 30 DW1000 Datasheet ver 2.17.
    # Average consumption coefficients considered.
    e_tx_preamble=83.0
    e_tx_data = 52.0
    e_rx_hunting  = 113.0
    e_rx_preamble = 113.0
    e_rx_data = 118.0
    e_idle = 18.0
    ref_voltage = 3.3 #V

    e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data = tuple(map(lambda curr_mA: curr_mA / 1e3 * ref_voltage,\
                                                                                 (e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data)))
    traces_pd.drop("epoch", axis=1, inplace=True)

    traces_pd["idle"]        = traces_pd["idle"]        * e_idle
    traces_pd["tx_preamble"] = traces_pd["tx_preamble"] * e_tx_preamble
    traces_pd["tx_data"]     = traces_pd["tx_data"]     * e_tx_data
    traces_pd["rx_hunting"]  = traces_pd["rx_hunting"]  * e_rx_hunting
    traces_pd["rx_preamble"] = traces_pd["rx_preamble"] * e_rx_preamble
    traces_pd["rx_data"]     = traces_pd["rx_data"]     * e_rx_data

    traces_pd["e_total"] = traces_pd["idle"] +\
        traces_pd["tx_preamble"] + traces_pd["tx_data"] +\
        traces_pd["rx_hunting"] + traces_pd["rx_preamble"] + traces_pd["rx_data"]
    statetime = traces_pd[["node", "e_total"]].groupby("node").agg(["mean", "std"]).reset_index()
    statetime.columns = ["node", "e_total_mean", "e_total_sd"]
    statetime.to_csv(savedir.joinpath("pernode_energy.csv"), index=False)

def get_sync_misses(boot_pd, sink_id, savedir=Path(".")):
    boot_pd = boot_pd.copy()
    boot_pd = boot_pd[boot_pd.node != sink_id] # discard sink

    # just a check. If nmisses > 3 (hard-coded atm), nodes will
    # scan again. This should not really happen, it should be extremely
    # rare. Still, if it happens, SCREAM and report it to the user.
    max_misses = boot_pd.nmisses.max()
    if  max_misses > 2:
        print(boot_pd[boot_pd.nmisses >= max_misses])
        logger.error("Some node has missed more than 2 bootstraps in a row! This is bad!")

    # count the number of nodes that missed the bootstrap in
    # a given epoch
    sync_miss = boot_pd[["epoch", "booted"]]
    sync_miss = sync_miss[sync_miss["booted"] == 0].groupby("epoch").agg({"booted": "size"}).reset_index()
    sync_miss.columns = ["epoch", "noboot"]
    sync_miss.to_csv(savedir.joinpath("noboot_epoch.csv"), index=False)

def run_log_parser(args):
    # parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("log_files", nargs="+",\
            help="The log file to parse")
    parser.add_argument("--force", "-f", help="Force the execution of parsing process",\
                        action="store_true", default=False)
    parser.add_argument("--plot", "-p", help="Plot a specific epoch",\
                        type=int)
    parser.add_argument("--interactive", "-i", help="Plot the interactive chart",\
                        action="store_true", default=False)
    parser.add_argument("--show", "-s", help="Show information on the log file processed",\
                        action="store_true")
    parser.add_argument("--start-epoch", "-se", dest="start_epoch", help="Define start epoch to be considered during processing",\
                        type=int)
    parser.add_argument("--last-epoch" , "-le", dest="last_epoch" , help="Define start epoch to be considered during processing",\
                        type=int)
    parser.add_argument("--nostrict", "-nos", help="Set loose severity",\
                        action="store_false", default=True)
    args = parser.parse_args(args)

    global SEVERITY_STRICT
    SEVERITY_STRICT = args.nostrict

    for log_file in args.log_files:

        log_file = Path(log_file).resolve()
        if not (log_file.exists() and log_file.is_file()):
            continue
        SAVEDIR  = log_file.parent.joinpath("stats")
        simconf_fp = log_file.parent.joinpath(Path("../simulation.conf")).resolve()
        stat_fp   = SAVEDIR.joinpath("stats.json")
        tsm_slots_fp = SAVEDIR.joinpath("tsm_slots.csv")
        cslots_fp = SAVEDIR.joinpath("cslots.csv")
        nslots_fp = SAVEDIR.joinpath("nslots.csv")
        origs_fp  = SAVEDIR.joinpath("originators.csv")
        acks_fp   = SAVEDIR.joinpath("acks.csv")
        errs_fp   = SAVEDIR.joinpath("errs.csv")
        stats_lastrx_fp = SAVEDIR.joinpath("stats_lastrx.csv")
        sent_fp   = SAVEDIR.joinpath("sent.csv")
        rcvd_fp   = SAVEDIR.joinpath("rcvd.csv")
        statetime_traces_fp = SAVEDIR.joinpath("statetime_traces.csv")
        rstats_fp   = SAVEDIR.joinpath("rstats.csv")
        boot_fp   = SAVEDIR.joinpath("boot.csv")
        summary_fp   = SAVEDIR.joinpath("summary.csv")

        # TODO: this is not stricly required, don't ask to reprocess log if missing
        mismatches_fp = SAVEDIR.joinpath("mismatch.csv")

        with open(simconf_fp, "r") as fh:
            simconf = Params(**json.load(fh))

        if not (SAVEDIR.exists() and SAVEDIR.is_dir()):
            os.makedirs(str(SAVEDIR))

        if not (sent_fp.exists() and sent_fp.is_file()) or\
           not (rcvd_fp.exists() and rcvd_fp.is_file()) or\
           not (statetime_traces_fp.exists() and statetime_traces_fp.is_file()) or\
           not (rstats_fp.exists()and rstats_fp.is_file()) or\
           not (boot_fp.exists()  and boot_fp.is_file()) or\
           not (cslots_fp.exists()and cslots_fp.is_file()) or\
           not (nslots_fp.exists()and nslots_fp.is_file()) or\
           not (stat_fp.exists()  and stat_fp.is_file()) or\
           not (acks_fp.exists()  and acks_fp.is_file()) or\
           not (origs_fp.exists() and origs_fp.is_file()) or\
           not (tsm_slots_fp.exists() and tsm_slots_fp.is_file()) or\
           not (errs_fp.exists() and errs_fp.is_file()) or\
           args.force is True:

            stats, sent_pd, rcvd_pd, cslots_pd, nslots_pd, origs_pd, acks_pd, tsm_slots_pd, errs_pd, mismatches_pd, statetime_traces_pd, rstats_pd, boot_pd = parse_log(log_file, parser=povo_parser, ignore_slots=False)

            sent_pd.to_csv  (sent_fp,   index=False)
            rcvd_pd.to_csv  (rcvd_fp,   index=False)
            cslots_pd.to_csv(cslots_fp, index=False)
            nslots_pd.to_csv(nslots_fp, index=False)
            acks_pd.to_csv  (acks_fp,   index=False)
            origs_pd.to_csv (origs_fp,  index=False)
            errs_pd.to_csv  (errs_fp,   index=False)
            statetime_traces_pd.to_csv (statetime_traces_fp,  index=False)
            rstats_pd.to_csv(rstats_fp, index=False)
            mismatches_pd.to_csv(mismatches_fp, index=False)
            tsm_slots_pd.to_csv (tsm_slots_fp,  index=False)

            # fill in missing epochs from boot entries
            # by default, nodes have not booted (0 in 3rd column)
            sim_epochs = stats["epochs"]
            skeleton_boot = pd.DataFrame([(node, epoch, 0, np.nan, np.nan) for epoch, node in product(sim_epochs, simconf.nodes)],
                                         columns=["node", "epoch", "booted", "nmisses", "leaked"]).sort_values(["epoch", "node"])
            skeleton_pd = pd.merge(skeleton_boot, boot_pd, how="outer", on=["node", "epoch"], suffixes=("_s", "_d"), validate="one_to_one")
            boot_pd = skeleton_pd[["node", "epoch"]].copy()
            # pick values from skeleton (s_*) if no data entry (d_*) for that epoch is found (is nan)
            boot_pd["booted"]  = skeleton_pd.apply(lambda r: r.booted_s if np.isnan(r.booted_d) else r.booted_d, axis=1)
            boot_pd["nmisses"] = skeleton_pd.apply(lambda r: r.nmisses_s if np.isnan(r.nmisses_d) else r.nmisses_d, axis=1)
            boot_pd["leaked"]  = skeleton_pd.apply(lambda r: r.leaked_s if np.isnan(r.leaked_d) else r.leaked_d, axis=1)
            boot_pd.to_csv  (boot_fp,   index=False)
            sim_epochs = None # just to be sure it does not interfere with later processing

            with open(stat_fp, "w") as fh:
                json.dump(stats, fh, indent=2)
        else:
            stats, sent_pd, rcvd_pd, cslots_pd, nslots_pd, origs_pd, acks_pd, statetime_traces_pd, rstats_pd, boot_pd = read_stats(SAVEDIR)

        with open(stat_fp, "r") as fh:
            stats_dict = json.load(fh)
            stats = Params(**stats_dict) # TODO: replace with advanced params

        if args.show is True:
            print(json.dumps(stats_dict, indent=4))
            sys.exit(0)

        if args.plot:
            if not args.plot in stats.epochs:
                raise ValueError(f"Epoch {args.plot} not present")

            if len(cslots_pd) == 0:
                raise ValueError("No log on slot details has been found. Verbose logs must be enabled when running experiments.")

            plot_epoch(cslots_fp, tsm_slots_fp, nodes=stats.nodes, bitmap_order=stats.bitmap_order, epoch=args.plot,\
                       savedir=str(SAVEDIR), interactive=args.interactive)

        origs_pd = pd.read_csv(origs_fp)
        offset = int(60 * 1/0.5)                                    # remove the last minute of simulation
        ref_epochs_pd = sent_pd
        if len(ref_epochs_pd) == 0:
            ref_epochs_pd = acks_pd

        if args.start_epoch:
            min_epoch = args.start_epoch
        else:
            min_epoch = 0

        if args.last_epoch:
            max_epoch = args.last_epoch
        else:
            max_epoch = ref_epochs_pd["epoch"].max()

        ref_epochs_pd = ref_epochs_pd[ (ref_epochs_pd["epoch"] >= min_epoch) & (ref_epochs_pd["epoch"] <= max_epoch) ]
        sim_epochs = sorted(ref_epochs_pd["epoch"].unique())[:-offset]
        print(f"Epochs considered: {len(sim_epochs)}, min {sim_epochs[0]}, max {sim_epochs[-1]}\n{'-'*40}")

        origs_pd = origs_pd[origs_pd["epoch"].isin(sim_epochs)]
        nslots_pd = nslots_pd[nslots_pd["epoch"].isin(sim_epochs)]
        sent_pd  = sent_pd[sent_pd["epoch"].isin(sim_epochs)]
        rcvd_pd  = rcvd_pd[rcvd_pd["epoch"].isin(sim_epochs)]
        acks_pd  = acks_pd[acks_pd["epoch"].isin(sim_epochs)]
        err_pd   = get_tsm_error(tsm_slots_fp, simconf.sink_id)
        err_pd   = err_pd[err_pd["epoch"].isin(sim_epochs)]
        statetime_traces_pd = statetime_traces_pd[statetime_traces_pd["epoch"].isin(sim_epochs)]
        rstats_pd= rstats_pd[rstats_pd["epoch"].isin(sim_epochs)]
        boot_pd  = boot_pd[boot_pd["epoch"].isin(sim_epochs)]

        get_statetime_info(statetime_traces_pd, savedir=SAVEDIR)
        sync_miss_pd = get_sync_misses(boot_pd, simconf.sink_id, savedir=SAVEDIR)

        if simconf.n_orig == 0:
            # set 0 originators on every epoch found in sim_epochs
            origs_pd = pd.DataFrame(((epoch, "0" * len(stats.bitmap_order)) for epoch in sim_epochs), columns=origs_pd.columns)

        origs_pd["n_originators"] = origs_pd["originators"].apply(lambda x: len(bitmap_apply(x, stats.bitmap_order)))
        # report the originators in a readable way (NB: not to be used for later processing)
        n_origs_pd = origs_pd.copy()
        n_origs_pd["originators"] = origs_pd["originators"].apply(lambda x: bitmap_apply(x, stats.bitmap_order))
        n_origs_pd.to_csv(str(SAVEDIR.joinpath("inspect_originators.csv")), index=False)
        n_origs_pd = None # force deallocation (hopefully)
        if len(origs_pd.n_originators.unique()) != 1 and SEVERITY_STRICT is True:
            tmp = origs_pd[origs_pd.n_originators != simconf.n_orig]
            raise ValueError("Eterogeneous number of originators across epochs.\n%s" % tmp.head())

        arate_pd2 = get_ack_rate2(sent_pd, rcvd_pd, simconf.n_orig, simconf.sink_id, savedir=SAVEDIR)
        arate_pd2 = arate_pd2[arate_pd2["node"] == simconf.sink_id]

        arate_pd = get_ack_rate(acks_pd, origs_pd, stats)
        arate_pd = arate_pd[(arate_pd["node"] == simconf.sink_id)]
        losses_pd = arate_pd[(arate_pd["a_rate"] < 1.00)]\
            .to_csv(str(SAVEDIR.joinpath("epoch_with_losses.csv")), index=False)
        bitmap_pdr = arate_pd.a_rate.mean()
        pkt_pdr    = arate_pd2.pdr.mean()
        if bitmap_pdr != pkt_pdr:
            logger.info("The PDR computed from acked bitmaps and the one computed with packets differ.")
        logger.info(f"ACK PDR: {bitmap_pdr}, PKT PDR: {pkt_pdr}")

        plot_arate(arate_pd, simconf.sink_id, simconf.n_orig, len(simconf.nodes))
        plt.savefig(str(SAVEDIR.joinpath("a_rate.pdf")))

        plot_nslots(nslots_pd, simconf.n_orig, len(simconf.nodes))
        plt.savefig(str(SAVEDIR.joinpath("nslots.pdf")))

        if len(cslots_pd):
            stat_before_last = get_stats_lastrx(simconf.sink_id, sim_epochs, cslots_fp, tsm_slots_fp)
            stat_before_last.to_csv(str(stats_lastrx_fp), index=False)
        else:
            logger.warn(f"No logs on slot details found. No {stats_lastrx_fp.name} generated...")


        summary_pd = summarize(simconf.sink_id, sent_pd, rcvd_pd, nslots_pd, origs_pd, acks_pd, stats)
        summary_pd.to_csv(str(summary_fp), index=False)
        summary = summary_pd.to_dict(orient="row")[-1]

        summary["sink_id"]   = simconf.sink_id
        summary["n_epochs"]  = len(origs_pd.epoch.unique())
        summary["epoch_min"] = origs_pd.epoch.min()
        summary["epoch_max"] = origs_pd.epoch.max()
        summary["n_nodes"]   = len(stats.nodes)
        summary["n_origs"]   = origs_pd.n_originators.unique()[0]
        print(summary)


SEVERITY_STRICT = False # set to False for mobility tests
if __name__ == "__main__":
    import sys
    import sys
    import argparse

    from povo_parser import match_filter, parse_node_logs as povo_parser
    from utility import Params

    SEVERITY_STRICT = True # set to False for mobility tests
    run_log_parser(sys.argv[1:])
