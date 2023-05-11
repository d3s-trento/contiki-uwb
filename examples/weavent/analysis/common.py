import os
import subprocess
import pandas as pd

ANALYSIS_PATH = os.path.dirname(os.path.realpath(__file__))
CACHE_ANALYSIS_PATH = os.path.join(os.getenv('PROJECT_DIR'), '.cache/analysis')


def read_dataframe(path: str) -> pd.DataFrame:
    if os.path.exists(path + '.fea'):
        return pd.read_feather(path + '.fea')
    elif os.path.exists(path + '.csv'):
        return pd.read_csv(path + '.csv')

    raise Exception("Neither feather, neither csv exists")


def run_awk_scripts(id: int, force=False):
    if force or not os.path.isdir(os.path.join(CACHE_ANALYSIS_PATH, f'{id}')):
        subprocess.Popen([os.path.join(ANALYSIS_PATH, 'analysis.sh'), f"{id}"]).wait()


def get_event_data(id: int, force=False):
    run_awk_scripts(id, False) # force) # TODO: TO fix

    return read_dataframe(os.path.join(CACHE_ANALYSIS_PATH, f'{id}', f'{id}'))


def get_tx_fs_data(id: int, force=False):
    run_awk_scripts(id, force)

    return read_dataframe(os.path.join(CACHE_ANALYSIS_PATH, f'{id}', f'tx_fs{id}'))


def get_tsm_slots(id: int, force=False):
    run_awk_scripts(id, force)

    return read_dataframe(os.path.join(CACHE_ANALYSIS_PATH, f'{id}', f'tsm{id}'))


def get_event_sent_data(id: int, force=False):
    run_awk_scripts(id, force)

    return read_dataframe(os.path.join(CACHE_ANALYSIS_PATH, f'{id}', f'event{id}'))


def get_synch_data(id: int, force=False):
    run_awk_scripts(id, False)  # force) TODO: Fix

    return read_dataframe(os.path.join(CACHE_ANALYSIS_PATH, f'{id}', f'e{id}'))


def get_complete_epochs(epochData: pd.DataFrame, num_of_nodes: int):
    w = epochData.groupby('epoch').nunique('node_id') == num_of_nodes
    return set(w.index[w['node_id']])


def filter_by_complete_synch(eventData: pd.DataFrame, epochData: pd.DataFrame, num_of_nodes: int):
    w = epochData.groupby('epoch').nunique('node_id') == num_of_nodes
    complete_flood_epochs = set(w.index[w['node_id']])

    return eventData[eventData['epoch'].isin(complete_flood_epochs)]


def filter_by_epoch(data: pd.DataFrame, min: int, max: int):
    if max < 0:
        max = data['epoch'].max()+max

    return data[(data['epoch'] > min) & (data['epoch'] < max)]


def compute_time_diff(eventData, rx_guard=0):
    eventData['diff'] = [min(a-e, 2**32-1-e+a, key=abs) for e, a in zip(eventData['expected'], eventData['actual'])]
    eventData['diff'] = eventData['diff']*0.000004
    eventData['diff'] = eventData['diff'] - rx_guard

    return eventData


def get_rel_by_node(eventData, completeSynchFilter=True):
    return eventData.groupby('node_id').nunique()['epoch']


def get_rel_by_epoch_ee(eventData, eventSentData, completeSynchFilter=True):
    res = eventSentData.groupby('epoch').nunique()['node_id'].reset_index().merge(eventData.groupby('epoch').nunique()['node_id'].reset_index(), on='epoch', how='outer')
    res['node_id_x'].fillna(0, inplace=True)
    res['node_id_y'].fillna(0, inplace=True)
    res['node_id'] = res['node_id_x'] + res['node_id_y']

    del res['node_id_x']
    del res['node_id_y']

    return res.reset_index().sort_values('epoch').set_index('epoch')


def get_rel_by_node_ee(eventData, eventSentData, completeSynchFilter=True):
    received = eventData.groupby('node_id').nunique()['epoch'].reset_index()
    sent = eventSentData.groupby('node_id').nunique()['epoch'].reset_index()

    res = sent.merge(received, on='node_id', how='outer')
    res['epoch_x'].fillna(0, inplace=True)
    res['epoch_y'].fillna(0, inplace=True)
    res['epoch'] = res['epoch_x'] + res['epoch_y']

    del res['epoch_x']
    del res['epoch_y']

    return res.reset_index().sort_values('node_id').set_index('node_id')
