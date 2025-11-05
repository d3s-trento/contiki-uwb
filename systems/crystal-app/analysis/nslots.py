#!/usr/bin/env python3
import argparse
import subprocess
from io import StringIO

import pandas as pd

import filtering_utilities as fu

parser = argparse.ArgumentParser(
        description='Calculate the reliability of crystal given a number of originators'
)

# TODO: Needed log in the application to do this
#parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
#parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

fu.add_epoch_filter(parser, True, +35, -10)

parser.add_argument('job_id', type=int, help='the id of the log to use')  #TODO: TMP integrate with the rest
parser.add_argument('--json', action='store_true', default=False, help='Output as json')  #TODO: TMP integrate with the rest

args = parser.parse_args()

p = subprocess.Popen(['get_nslots.sh', f'{args.job_id}'], stdout=subprocess.PIPE)
sio = StringIO(p.communicate()[0].decode('utf-8'))
data = pd.read_csv(sio)

data = fu.apply_epoch_filter(args, data)

original_len = len(data)
data = data.drop_duplicates(['job_id', 'epoch', 'node_id'], keep='first')
if len(data) != original_len:
    print('ERR: There are some duplicates for <job_id, epoch>')

epochs_df = data[['job_id', 'epoch']]
epochs_df = epochs_df.drop_duplicates(['job_id', 'epoch'])

data_min = data.groupby(['job_id','epoch']).min().reset_index()['nslot'].mean()
data_mean = data['nslot'].mean()
data_max = data.groupby(['job_id', 'epoch']).max().reset_index()['nslot'].mean()

if not args.json:
    print('Epochs: {}'.format(len(epochs_df)))
    print('Mean of min: {}'.format(data_min))
    print('Mean: {}'.format(data_mean))
    print('Mean of max: {}'.format(data_max))
else:
    import json
    print(json.dumps({"n_epochs": len(epochs_df), "last_slot_avg": data_mean}))
    #print(f'{args.job_id},{data_min},{data_mean},{data_max}')
