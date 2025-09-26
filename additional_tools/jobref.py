from abc import ABC, abstractmethod
import os
import stat
import argparse

import pandas as pd

import logging

import subprocess
from io import StringIO


class JobLog(ABC):
    def __init__(self):
        pass

    @abstractmethod
    def get_label(self):
        pass


class JobLogId(JobLog):
    def __init__(self, id: int):
        self.id = id

    def __str__(self):
        return f'job{{{self.id}}}'

    def __repr__(self):
        return f'job{{{self.id}}}'

    def get_zip_path(self) -> str:
        if os.environ.get('JOBS_DIR') is None:
            raise Exception('PROJECT_DIR is not set')

        return os.path.join(os.environ['JOBS_DIR'], f'job_{self.id}.tar.gz')

    def get_cache_dir(self):
        if os.environ.get('PROJECT_DIR') is None:
            raise Exception('PROJECT_DIR is not set')

        return os.path.join(os.environ['PROJECT_DIR'], '.cache', 'analysis', f'{self.id}')

    def create_cache_dir(self) -> None:
        from pathlib import Path

        Path(self.get_cache_dir()).mkdir( parents=True, exist_ok=True )

    def get_label(self):
        return f'job_{self.id}'

    def create_folder_structure(self):
        if not os.path.exists(self.get_cache_dir()):
            os.makedirs(self.get_cache_dir())

    def save_csv(self, df: pd.DataFrame, filename: str):
        self.create_folder_structure()
        df.to_csv(os.path.join(self.get_cache_dir(), filename), index=False)

    def access(self, force: bool, num_nodes=None):
        a = JobLogIdAccessor(self, force=force, num_nodes=num_nodes)
        #TODO: Stuff
        return a


import typing

def read_dataframe(path: str) -> pd.DataFrame:
    if os.path.exists(path + '.fea'):
        return pd.read_feather(path + '.fea')
    elif os.path.exists(path + '.csv'):
        return pd.read_csv(path + '.csv')

    raise Exception("Neither feather, neither csv exists")


def cache_file(filename: str, *, to_save: bool = True, save_format='feather'):
    def inner(func):
        def wrapper(self, *args, **kwargs):
            path = os.path.join(self.job_id.get_cache_dir(), filename.format(id=self.job_id.id, **self.__dict__))

            if not self.force and os.path.exists(path + '.fea'):
                return pd.read_feather(path + '.fea')
            elif not self.force and os.path.exists(path + '.csv'):
                return pd.read_csv(path + '.csv')

            # force or neither files exist
            data = func(self,*args, **kwargs)

            if to_save:
                self.job_id.create_cache_dir()

                if save_format == 'feather':
                    data.reset_index(drop=True).to_feather(path + '.fea')
                elif save_format == 'csv':
                    data.to_csv(path + '.csv')
                else:
                    raise Exception("Required save format not recognized")

            return data

        return wrapper

    return inner


class JobLogIdAccessor:
    def __init__(self, job_id: JobLogId, force: bool = True, num_nodes=None):
        self.job_id = job_id
        self.force = force

        if num_nodes is None:
            p = subprocess.Popen(['get_num_nodes.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

            sio = p.communicate()[0].decode('utf-8')

            num_nodes = int(sio)-1

            print(f'Automatically detected the number of nodes as {num_nodes} ({self.job_id.id})')

        self.num_nodes = num_nodes

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        pass

    @property
    @cache_file('tsm{id}', to_save=True)
    def tsm_slots(self) -> pd.DataFrame:
        p = subprocess.Popen(['get_tsm_slots.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        return pd.read_csv(sio)

    @property
    @cache_file('completed_epochs{num_nodes}', to_save=True)
    def completed_epochs(self) -> pd.DataFrame:
        data = self.tsm_slots.groupby('epoch').nunique().reset_index()
        data = data[data['node_id'] == self.num_nodes + 1]

        return data['epoch'].to_frame(name='epoch')

    @property
    @cache_file('statetime_traces.csv', to_save=True)
    def statetime_traces(self) -> pd.DataFrame:
        p = subprocess.Popen(['get_statetime_traces.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        return pd.read_csv(sio)

    @property
    @cache_file('energy.csv', to_save=True)
    def energy(self) -> pd.DataFrame:
        MAX_T = 4e6

        traces_pd = self.statetime_traces

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

        return traces_pd


class JobLogFile(JobLog):
    def __init__(self, path: str):
        self.path = path

    def get_label(self):
        return self.path

    def __str__(self):
        return f'path{{{self.path}}}'

    def __repr__(self):
        return f'path{{{self.path}}}'


class JobLogDataFrameWrapper(JobLog):
    def __init__(self, df: pd.DataFrame):
        self.pd = pd

    def __str__(self):
        return f'DataFrame{{}}'

    def __repr__(self):
        return f'DataFrame{{}}'

    def get_label(self):
        return '[DataFrame]'


def from_job_log(x: str, expected_file_name: str):
    if x.isdigit():
        return JobLogId(int(x))

    if os.path.exists(x) and (os.path.isfile(x) or stat.S_ISFIFO(os.stat(x).st_mode)):
        if os.path.isfile(x) and os.path.basename(x) != expected_file_name:
            logging.warn('The name of the passed file is different from expected, are you sure?')

        return JobLogFile(x)

    raise argparse.ArgumentTypeError('Expected a job_id or a path or pipe containing the necessary data')
