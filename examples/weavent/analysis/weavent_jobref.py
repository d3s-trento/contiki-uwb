import jobref
import common as c

import pandas as pd
import numpy as np

import subprocess
from io import StringIO

import filtering_utilities as fu

def from_weavent_job_log(a: str, expected_file_name: str):
    v = jobref.from_job_log(a, expected_file_name)

    if isinstance(v, jobref.JobLogId):
        return WeaventLogId(v.id)

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


class WeaventLogId(jobref.JobLogId):
    def __init__(self, id: int):
        super().__init__(id)

    def access(self, force: bool, rx_guard=0, num_nodes=33, fp_guard=0.2) -> 'WeaventLogIdAccessor':
        return WeaventLogIdAccessor(self, force, rx_guard, num_nodes, fp_guard)

class WeaventLogIdAccessor(jobref.JobLogIdAccessor):
    def __init__(self, job_id, force, rx_guard, num_nodes, fp_guard):
        super().__init__(job_id, force, num_nodes)

        self.rx_guard = rx_guard
        #self.num_nodes = num_nodes
        self.fp_guard = fp_guard

    @property
    @jobref.cache_file('{id}', to_save=True)
    def event_data(self):
        if self.job_id.id > 10242:
            raise Exception("No longer supported")

        p = subprocess.Popen(['get_time.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {'node_id': np.uint8, 'epoch': np.uint16, 'repetition_i': np.uint8, 'expected': np.uint32, 'actual': np.uint32})

    @property
    @jobref.cache_file('d{id}', to_save=True)
    def time_event_data(self):
        #if (self.job_id.id > 10242) or (self.job_id.id < 2000):
        p = subprocess.Popen(['get_k_time.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        dd = convert_type(dd, {'node_id': np.uint8, 'epoch': np.uint16, 'repetition_i': np.uint8, 'diff': np.uint32})
        dd['diff'] /= 1000000

        return dd

        #dd = self.event_data

        #dd['diff'] = (dd['actual'] - dd['expected'])*0.000004
        #dd['diff'] -= self.rx_guard

        #dd.drop(['expected', 'actual'], axis=1)

        #dd = dd.astype({'diff': np.float32})  # TODO: Probably can use even float16

        #return dd

    @property
    @jobref.cache_file('falsePositives{fp_guard}')
    def false_positives(self):
        dd = self.time_event_data

        dd.sort_values(['epoch', 'repetition_i', 'diff'], inplace=True)

        falsePositives = set()

        dd['closestDiff'] = dd.groupby(['epoch', 'repetition_i'])['diff'].diff()

        dd['mask'] = dd['closestDiff'] > self.fp_guard
        dd['mask'] = (dd.groupby(['epoch', 'repetition_i'])['mask'].cumsum() != 0)

        dd = dd[dd['mask']]
        dd.drop(['mask'], axis=1, inplace=True)

        return dd

    @property
    @jobref.cache_file('e{id}', to_save=True)
    def bootstrap_data(self):
        p = subprocess.Popen(['get_bootstrap.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {'node_id': np.uint8, 'epoch': np.uint16, 'hop': np.uint8})

    @property
    @jobref.cache_file('incomplete_bootstrap{num_nodes}')
    def incomplete_bootstrap(self):
        dd = self.bootstrap_data

        completeEpochs = c.get_complete_epochs(dd, self.num_nodes + 1)

        dd = dd['epoch'].drop_duplicates()

        return dd[~dd.isin(completeEpochs)].to_frame(name='epoch')


    @property
    @jobref.cache_file('complete_bootstrap{num_nodes}', to_save=True)
    def complete_bootstrap(self) -> pd.DataFrame: # TODO: Should be moved to be inside base jobref
        return pd.Series(list(fu.get_complete_epochs(self.bootstrap_data, self.num_nodes+1))).to_frame(name='epoch')

    @property
    @jobref.cache_file('tx_fs', to_save=True)
    def tx_fs(self):
        p = subprocess.Popen(['get_tx_fs.sh', f'{self.job_id.id}'], stdout=subprocess.PIPE)

        sio = StringIO(p.communicate()[0].decode('utf-8'))

        dd = pd.read_csv(sio)

        return convert_type(dd, {'node_id': np.uint8, 'epoch': np.uint16, 'repetition_i': np.uint8})


