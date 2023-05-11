#!/usr/bin/env python3
import sys

import pandas as pd

def main():
    data = pd.read_csv(sys.stdin.buffer)
    data.reset_index(drop=True, inplace=True)
    data.to_feather(sys.stdout.buffer)

if __name__ == "__main__":
    main()
