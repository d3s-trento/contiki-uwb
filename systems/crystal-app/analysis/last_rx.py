#!/usr/bin/env python3
import argparse

import pandas as pd

import filtering_utilities as fu

parser = argparse.ArgumentParser(
        description='Calculate the reliability of crystal given a number of originators'
)

# TODO: Needed log in the application to do this
#parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
#parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

fu.add_epoch_filter(parser, True, +35, -10)

parser.add_argument('log', type=str, help='the log to use')  #TODO: TMP integrate with the rest

args = parser.parse_args()

data = pd.read_csv(args.log)

data = fu.apply_epoch_filter(args, data)

original_len = len(data)
data = data.drop_duplicates(['job_id', 'epoch'], keep='first')
if len(data) != original_len:
    print('ERR: There are some duplicates for <job_id, epoch>')

print('Epochs: ', len(data))
print('Mean: ', data['last_rx'].mean())
print(data['last_rx'].describe().to_frame().T)
