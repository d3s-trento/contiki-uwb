#!/usr/bin/env python3
import json

from pathlib import Path

import numpy as np
import pandas as pd

from log_parser2 import get_ack_rate2, get_rx_slots
from utility import Params


STATS = "stats.json"
SIMCONF = "simulation.conf"
ORIGINATORS = "originators.csv"
RCVD = "rcvd.csv"
SENT = "sent.csv"
SUMMARY = "summary.csv"
PERNODE_ENERGY = "pernode_energy.csv"
NOBOOT_EPOCH   = "noboot_epoch.csv"

if __name__ == "__main__":
    import argparse

    # parse arguments
    parser = argparse.ArgumentParser()
    parser.add_argument("log_files", nargs="+",\
            help="The log file to parse")
    parser.add_argument("--dest_file", "-d", nargs="?",\
            help="The file collected data are saved to", default=None)
    args = parser.parse_args()


    # delivered == acked here
    header = ["n_node", "sink", "n_senders", "n_epochs", "n_pkt", "n_delivered",
            "slot_first_rx", "slot_last_rx", "slot_avg_rx",
            "nslot", "nslot_sink", "nslot_last_new",
            "esink_mean", "esink_sd",
            "epeers_mean", "epeers_sd",
            "energy_mean", "energy_sd",
            "sink_pdr"]

    noboot_header = ["n_node", "sink", "n_senders", "n_epochs", "noboot", "freq"]
    # for each configuration (n_node, sink, n_senders)
    # we want a single record with
    # <n_node, sink, n_senders, n_epochs, n_pkt, n_delivered, sink_pdr, n_slot_sink>
    data = []
    noboot_data = []
    pernode_energy_data = []
    for log_file in args.log_files:

        log_file = Path(log_file).resolve()
        if not (log_file.exists() and log_file.is_file()):
            continue
        parent = log_file.parent.resolve()

        # extract the only value of a given column
        pick_value = lambda df, field: df[field].tolist()[0]

        with open(str(parent.joinpath("./stats/%s" % STATS)), "r") as fh:
            stats = json.load(fh)
        with open(str(parent.joinpath("../%s" % SIMCONF)), "r") as fh:
            simconf = json.load(fh)

        origs = pd.read_csv(str(parent.joinpath("./stats/%s" % ORIGINATORS)))
        rcvd  = pd.read_csv(str(parent.joinpath("./stats/%s" % RCVD)))
        sent  = pd.read_csv(str(parent.joinpath("./stats/%s" % SENT)))
        summary = pd.read_csv(str(parent.joinpath("./stats/%s" % SUMMARY)))
        energy  = pd.read_csv(str(parent.joinpath("./stats/%s" % PERNODE_ENERGY)))
        noboot  = pd.read_csv(str(parent.joinpath("./stats/%s" % NOBOOT_EPOCH)))

        # consider only epochs in the summary file (which is already filtered)
        epochs_considered = set(summary.epoch[~summary.epoch.isna()].astype("int").unique())

        # pick aggregate stat (last entry of the dataframe)
        aggregate = summary.to_dict(orient="row")[-1]
        assert np.isnan(aggregate["epoch"]) # aggregate data has epoch field to NA

        nslot_sink, nslot_last_new = tuple([aggregate[f] for f in ("last_slot_sink", "last_new_pkt_slot_sink")])
        nslot = aggregate["last_slot_avg"]

        rcvd  = rcvd[rcvd.epoch.isin(epochs_considered)]
        sent  = sent[sent.epoch.isin(epochs_considered)]
        origs = origs[origs.epoch.isin(epochs_considered)]

        n_origs = simconf["n_orig"]
        sink_id = simconf["sink_id"]
        nodes = stats["nodes"]
        bitmap_order = stats["bitmap_order"]

        # get sink pkt stats
        arate = get_ack_rate2(sent, rcvd, n_origs, sink_id, log=False)
        _, n_acked, n_pkt, _ = tuple(arate[arate["node"] == sink_id].to_numpy()[0])
        n_acked, n_pkt = int(n_acked), int(n_pkt)

        slot_first_rx, slot_last_rx, slot_avg_rx = get_rx_slots(rcvd, sink_id)
        #print("%.3f, %.3f, %.3f" % (slot_first_rx, slot_last_rx, slot_avg_rx))

        energy_sink = energy[energy.node == sink_id]
        energy_peers = energy[energy.node != sink_id]
        esink_mean  = energy_sink.e_total_mean.tolist()[0]
        esink_sd    = energy_sink.e_total_sd.tolist()[0]
        epeers_mean  = energy_peers.e_total_mean.mean()
        epeers_sd    = energy_peers.e_total_sd.mean()
        energy_mean  = energy.e_total_mean.mean()
        energy_sd    = energy.e_total_sd.mean()

        t_max = energy[energy.e_total_mean == energy.e_total_mean.max()]
        t_min = energy[energy.e_total_mean == energy.e_total_mean.min()]
        ehnid_max  = t_max.node.tolist()[0]
        ehmean_max = t_max.e_total_mean.tolist()[0]
        ehnid_min  = t_min.node.tolist()[0]
        ehmean_min = t_min.e_total_mean.tolist()[0]
        #print(f"EH sink {sink_id}, U {n_origs}, MAX node_id {ehnid_max}, energy {ehmean_max}, MIN node_id {ehnid_min}, energy {ehmean_min}")

        n_nodes  = len(nodes)
        n_epochs = len(epochs_considered)
        entry = (n_nodes, sink_id, n_origs, n_epochs, n_pkt, n_acked,
                 slot_first_rx, slot_last_rx, slot_avg_rx,
                 nslot, nslot_sink, nslot_last_new,
                 esink_mean, esink_sd,
                 epeers_mean, epeers_sd,
                 energy_mean, energy_sd)
        data.append(entry)

        for noboot_v, freq in noboot[["noboot"]].groupby("noboot").agg("size").reset_index(name="freq").to_numpy():
            noboot_data.append((n_nodes, sink_id, n_origs, n_epochs, noboot_v, freq))

        for row in energy.to_dict(orient="row"):
            entry = (n_nodes, sink_id, n_origs, n_epochs, row["node"], row["e_total_mean"])
            pernode_energy_data.append(entry)

    
    # compute average pernode energy aggregated across different experiments
    energy_columns = ["n_node", "sink", "n_senders", "n_epochs", "node", "e_total_mean"]
    energy_df = pd.DataFrame(pernode_energy_data, columns=energy_columns)
    energy_df = energy_df.groupby(["n_node", "sink", "n_senders", "node"], as_index=False)\
        .agg({"n_epochs": sum, "e_total_mean": np.mean})
    # compute min and max energy among nodes of a given configuration (n nodes, sink, n senders)
    energy_df.drop(["node"], axis=1, inplace=True)
    print(energy_df.columns)
    #energy_min = energy_df.groupby(["n_node", "sink", "n_senders"]).e_total_mean.transform("min")
    #energy_max = energy_df.groupby(["n_node", "sink", "n_senders"]).e_total_mean.transform("max")
    energy_df = energy_df.groupby(["n_node", "sink", "n_senders"])\
        .agg({"n_epochs":"first", "e_total_mean": ["min", "max"]})\
        .reset_index()
    energy_df.columns = ["n_node", "sink", "n_senders", "n_epochs", "e_min", "e_max"]

    data_df = pd.DataFrame(data, columns=[f for f in header if f != "sink_pdr"])
    with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
        print(data_df)
    data_df = data_df.groupby(["n_node", "sink", "n_senders"], as_index=False)\
        .agg({"n_epochs": sum, "n_pkt": sum, "n_delivered": sum,
              "slot_first_rx": np.mean, "slot_last_rx": np.mean, "slot_avg_rx": np.mean,
              "nslot": np.mean, "nslot_sink": np.mean, "nslot_last_new": np.mean,
              "esink_mean": np.mean, "esink_sd": np.mean,
              "epeers_mean": np.mean, "epeers_sd": np.mean,
              "energy_mean": np.mean, "energy_sd": np.mean}) #TODO: does it make sense to compute the mean of the standard deviations :(

    data_df = pd.merge(data_df, energy_df, on=["n_node", "sink", "n_senders", "n_epochs"], how="outer", validate="one_to_one")

    for phase in ["sink", "last_new"]:
        data_df["nslot_%s" % phase] = data_df["nslot_%s" % phase].apply(lambda x: int(round(x)) if not np.isnan(x) else np.nan)

    # compute pdr
    data_df["sink_pdr"] = data_df.n_delivered / data_df.n_pkt

    data_df["nslot_last_new"] = data_df["nslot_last_new"].round(3)
    data_df["slot_first_rx"] = data_df["slot_first_rx"].round(3)
    data_df["slot_last_rx"] = data_df["slot_last_rx"].round(3)
    data_df["slot_avg_rx"] = data_df["slot_avg_rx"].round(3)
    data_df["esink_mean"] = data_df["esink_mean"].round(3)
    data_df["esink_sd"] = data_df["esink_sd"].round(3)
    data_df["epeers_mean"] = data_df["epeers_mean"].round(3)
    data_df["epeers_sd"] = data_df["epeers_sd"].round(3)
    data_df["energy_mean"] = data_df["energy_mean"].round(3)
    data_df["energy_sd"] = data_df["energy_sd"].round(3)
    data_df["e_min"] = data_df["e_min"].round(3)
    data_df["e_max"] = data_df["e_max"].round(3)


    DEFAULT_FILENAME = "experiments.txt"
    SEP = " "
    if args.dest_file:
        dfile = Path(args.dest_file).resolve()
        if dfile.exists() and dfile.is_dir():
            dfile = dfile.joinpath(DEFAULT_FILENAME)

        data_df.to_csv(str(dfile), index=False, sep=SEP)

    else:
        print(data_df.fillna("NA").to_csv(None, index=False, sep=SEP))

    # NOBOOT aggregator
    data_df = pd.DataFrame(noboot_data, columns=noboot_header)
    data_df = data_df.groupby(["n_node", "sink", "n_senders", "noboot"], as_index=False)\
        .agg({"n_epochs": sum,
              "freq": sum})

    DEFAULT_FILENAME = "noboot.txt"
    SEP = " "
    if args.dest_file:
        dfile = Path(args.dest_file).resolve()
        if dfile.exists() and dfile.is_dir():
            dfile = dfile.joinpath(DEFAULT_FILENAME)

        data_df.to_csv(str(dfile), index=False, sep=SEP)

    else:
        pass
        #print(data_df.to_csv(None, index=False, sep=SEP))
