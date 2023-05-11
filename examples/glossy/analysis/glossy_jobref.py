import jobref

import pandas as pd
import numpy as np

import subprocess
from io import StringIO

import filtering_utilities as fu


def from_weavent_job_log(a: str, expected_file_name: str):
    v = jobref.from_job_log(a, expected_file_name)

    if isinstance(v, jobref.JobLogId):
        return GlossyLogId(v.id)


def check_valid_type(col: pd.Series, type):
    info = np.iinfo(type)

    if len(col) == 0:
        return True

    if not pd.to_numeric(col, errors='coerce').notnull().all():
        print('coerce error')
        return False

    if not (col.min() >= info.min and col.max() <= info.max):
        print('range error')
        return False

    return True


def convert_type(data: pd.DataFrame, types: dict):
    for k, v in types.items():
        if not check_valid_type(data[k], v):
            raise Exception(f'{k} not in expected range')

    data = data.astype(types, copy=False)

    return data


class GlossyLogId(jobref.JobLogId):
    def __init__(self, id: int):
        super().__init__(id)

    def access(self, force: bool, num_nodes=33) -> 'GlossyLogIdAccessor':
        return GlossyLogIdAccessor(self, force, num_nodes)


class GlossyLogIdAccessor(jobref.JobLogIdAccessor):
    def __init__(self, job_id, force, num_nodes):
        super().__init__(job_id, force, num_nodes)

        #self.num_nodes = num_nodes

    @property
    @jobref.cache_file('latency{id}', to_save=True)
    def latency(self) -> pd.DataFrame:
        p = subprocess.Popen(['get_latency.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {
            'node_id': np.uint8,
            'epoch': np.uint16,
            'repetition_i': np.uint8,
            'duration': np.uint32
        })

    @property
    @jobref.cache_file('e{id}', to_save=True)
    def bootstrap_data(self) -> pd.DataFrame:
        p = subprocess.Popen(['get_bootstrap.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {
            'node_id': np.uint8,
            'epoch': np.uint16,
            'hop': np.uint8
        })

    @property
    @jobref.cache_file('complete_bootstrap{num_nodes}', to_save=True)
    def complete_bootstrap(self) -> pd.DataFrame:
        return pd.Series(list(fu.get_complete_epochs(self.bootstrap_data, self.num_nodes+1))).to_frame(name='epoch')

    @property
    @jobref.cache_file('tx_fs', to_save=True)
    def tx_fs(self):
        p = subprocess.Popen(['get_tx_fs.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {'node_id': np.uint8, 'epoch': np.uint16, 'repetition_i': np.uint8})


