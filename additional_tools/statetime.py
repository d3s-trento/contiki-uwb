import subprocess

import os
import pandas as pd

from jobref import JobLog, JobLogId, JobLogFile, JobLogDataFrameWrapper


def get_traces(joblog: JobLog, force=False) -> pd.DataFrame:
    if isinstance(joblog, JobLogFile):
        return pd.read_csv(joblog.path)
    elif isinstance(joblog, JobLogId):
        FILENAME = 'statetime_traces.csv'
        path = os.path.join(joblog.get_cache_dir(), FILENAME)

        if force or not os.path.exists(path):
            joblog.create_folder_structure()

            with open(path, 'w') as f:
                subprocess.run(["get_statetime_traces.sh", '{}'.format(joblog.id)],
                                stdout=f)

            return pd.read_csv(path)
        else:
            return pd.read_csv(path)

    raise Exception('Unrecognized type of JobLog')


def get_energy(traces: JobLog, minEpoch: int, maxEpoch: int, force=False) -> pd.DataFrame:
    FILENAME = 'energy.csv'

    if not force and isinstance(traces, JobLogId) and os.path.exists(os.path.join(traces.get_cache_dir(), FILENAME)):
        return pd.read_csv(os.path.join(traces.get_cache_dir(), FILENAME))

    if isinstance(traces, JobLogFile):
        return pd.read_csv(traces.path)

    if isinstance(traces, JobLogId):
        traces_pd = get_traces(traces, force=force)
    elif isinstance(traces, JobLogDataFrameWrapper):
        traces_pd = traces.df
    else:
        raise Exception('Unrecognized type of JobLog')

    MAX_T = 4e6
    
    if minEpoch is None:
        minEpoch = 20

    if maxEpoch is None:
        maxEpoch = traces_pd['epoch'].max() - 10

    # remove epoch where an underflow occurred
    mask = (traces_pd.idle > MAX_T) | (traces_pd.rx_hunting > MAX_T) |\
        (traces_pd.tx_preamble > MAX_T) | (traces_pd.tx_data > MAX_T) |\
        (traces_pd.rx_preamble > MAX_T) | (traces_pd.rx_data > MAX_T)
    epochs_to_remove = traces_pd[mask].epoch.unique()

    if len(epochs_to_remove) > 0:
        print("Found statetime underflows, removing the following epochs from energy computation:\n{}\nlen: {}\nepochs: {}".format(traces_pd[mask].head(), len(traces_pd[mask]), traces_pd[mask]['epoch'].unique()))

    traces_pd = traces_pd[~traces_pd.epoch.isin(epochs_to_remove)]

    # Configuration Considered: Ch 2, plen 128, 64Mhz PRF, 6.8Mbps Datarate
    # Setting N of page 30 DW1000 Datasheet ver 2.17.
    # Average consumption coefficients considered.
    e_tx_preamble = 83.0
    e_tx_data = 52.0
    e_rx_hunting = 113.0
    e_rx_preamble = 113.0
    e_rx_data = 118.0
    e_idle = 18.0
    ref_voltage = 3.3  # V

    e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data = tuple(map(lambda curr_mA: curr_mA / 1e3 * ref_voltage,\
                                                                                 (e_idle, e_tx_preamble, e_tx_data, e_rx_hunting, e_rx_preamble, e_rx_data)))

    traces_pd["idle"] =         traces_pd["idle"]        * e_idle
    traces_pd["tx_preamble"] =  traces_pd["tx_preamble"] * e_tx_preamble
    traces_pd["tx_data"] =      traces_pd["tx_data"]     * e_tx_data
    traces_pd["rx_hunting"] =   traces_pd["rx_hunting"]  * e_rx_hunting
    traces_pd["rx_preamble"] =  traces_pd["rx_preamble"] * e_rx_preamble
    traces_pd["rx_data"] =      traces_pd["rx_data"]     * e_rx_data

    traces_pd["e_total"] = traces_pd["idle"] +\
        traces_pd["tx_preamble"] + traces_pd["tx_data"] +\
        traces_pd["rx_hunting"] + traces_pd["rx_preamble"] + traces_pd["rx_data"]

    traces_pd = traces_pd[(traces_pd['epoch'] >= minEpoch)]
    traces_pd = traces_pd[(traces_pd['epoch'] <= maxEpoch)]

    if isinstance(traces, JobLogId):
        traces.save_csv(traces_pd, FILENAME)

    return traces_pd


def get_pernode_energy(energy: JobLog, maxEpoch: int, force=False) -> pd.DataFrame:
    FILENAME = 'pernode_energy.csv'
    if not force and isinstance(energy, JobLogId) and os.path.exists(os.path.join(energy.get_cache_dir(), FILENAME)):
        return pd.read_csv(os.path.join(energy.get_cache_dir(), FILENAME))

    if isinstance(energy, JobLogFile):
        return pd.read_csv(energy.path)

    if isinstance(energy, JobLogId):
        energy_pd = get_energy(energy, force=force)
    elif isinstance(energy, JobLogDataFrameWrapper):
        energy_pd = energy.df
    else:
        raise Exception('Unrecognized type of JobLog')

    energy_pd.drop("epoch", axis=1, inplace=True)

    statetime = energy_pd[["node_id", "e_total"]].groupby("node_id").agg(["mean", "std"]).reset_index()
    statetime.columns = ["node_id", "e_total_mean", "e_total_sd"]

    if isinstance(energy, JobLogId):
        energy.save_csv(statetime, FILENAME)

    return statetime
