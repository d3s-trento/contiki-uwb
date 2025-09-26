#!/usr/bin/env python3
import argparse

import pandas as pd

parser = argparse.ArgumentParser(
        description='Calculate the reliability of crystal given a number of originators'
)

# TODO: Needed log in the application to do this
#parser.add_argument('--no-synchFilter', dest='synchFilter', default=True, action='store_false', help='filter by only succesful synchs')
#parser.add_argument('--synchFilter', dest='synchFilter', default=True, action='store_true', help='filter by only succesful synchs')

parser.add_argument('--no-epochFilter', dest='epochFilter', default=False, action='store_false', help='filter by epoch')
parser.add_argument('--epochFilter',    dest='epochFilter', default=False, action='store_true',  help='filter by epoch')

parser.add_argument('log', type=str, help='the id of the log to use')  #TODO: TMP integrate with the rest

args = parser.parse_args()

data = pd.read_csv(args.log)

if args.epochFilter:
    data = data[(data.epoch < data['epoch'].max() - 10) & (data.epoch > data['epoch'].min() + 10)]

print('Epochs: {}'.format(len(set(data['epoch']))))
print('Mean of min: {}'.format(data.groupby('epoch').min().reset_index()['nslot'].mean()))
print('Mean: {}'.format(data['nslot'].mean()))
print('Mean of max: {}'.format(data.groupby('epoch').max().reset_index()['nslot'].mean()))
