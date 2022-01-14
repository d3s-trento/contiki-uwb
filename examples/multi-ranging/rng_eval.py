import os
import sys
import re
import csv
import argparse
import pandas as pd
import numpy as np
import deploymentMap as dep


def distance(a, b):
    return (
        ((a[0] - b[0]) ** 2) +
        ((a[1] - b[1]) ** 2)
    ) ** 0.5


def parse_file(log_file, results_file='ranging_results.csv', ts_file='ranging_ts.csv'):
    columns = ['init', 'resp', 'seqn', 'time_ms', 'success', 'dist', 'dist_bias',
               'err', 'err_bias', 'dist_ground_truth',
               'fp_pwr', 'rx_pwr', 'freq_offset']
    outfile = open(results_file, 'w')
    writer = csv.writer(outfile, dialect='excel')
    writer.writerow(columns)
    columns_ts = ['init', 'resp', 'seqn',
               'poll_tx_ts', 'poll_rx_ts',
               'resp_tx_ts', 'resp_rx_ts',
               'ds_final_tx_ts', 'ds_final_rx_ts']
    outfile_ts = open(ts_file, 'w')
    writer_ts = csv.writer(outfile_ts, dialect='excel')
    writer_ts.writerow(columns_ts)

    idByAddr = dict((v, k) for k, v in dep.addrById.items())
    if len(dep.addrById) != len(idByAddr):
        print("WARNING! Duplicate addresses in addrById.")
    coordById = dep.coordById

    # regular expressions to match
    # RNG [%lu/%lums] %x->%x SUCCESS %d bias %d fppwr %d rxpwr %d cifo %d"

    regex_rng = re.compile(r".*RNG \[(?P<seqn>\d+)/(?P<ms>\d+)ms\] "
                           r"(?P<init>\w\w\w\w)->(?P<resp>\w\w\w\w): SUCCESS "
                           r"(?P<dist>\d+) bias (?P<dist_bias>\d+) "
                           r"fppwr (?P<fppwr>-?\d+) "
                           r"rxpwr (?P<rxpwr>-?\d+) "
                           r"cifo (?P<cifo>-?\d+).*\n")

    regex_no = re.compile(r".*RNG \[(?P<seqn>\d+)/(?P<ms>\d+)ms\] "
                          r"(?P<init>\w\w\w\w)->(?P<resp>\w\w\w\w):.*FAIL.*\n")

    regex_ts = re.compile(r".*TS \[(?P<seqn>\d+)\] "
                          r"(?P<init>\w\w\w\w)->(?P<resp>\w\w\w\w): "
                          r"(?P<poll_tx_ts>\d+) (?P<poll_rx_ts>\d+) "
                          r"(?P<resp_tx_ts>\d+) (?P<resp_rx_ts>\d+) "
                          r"(?P<ds_final_tx_ts>\d+) (?P<ds_final_rx_ts>\d+).*\n")

    # open log and read line by line
    with open(log_file, 'r') as f:
        for line in f:

            # match transmissions strings
            m = regex_rng.match(line)
            if m:

                # get dictionary of matched groups
                d = m.groupdict()

                seqn = int(d['seqn'])
                time_ms = int(d['ms'])

                # retrieve IDs of nodes and the measured distance
                init = int(idByAddr[d['init'][:2] + ':' + d['init'][2:]])
                resp = int(idByAddr[d['resp'][:2] + ':' + d['resp'][2:]])
                dist = int(d['dist']) / 100.0
                dist_bias = int(d['dist_bias']) / 100.0
                fp_pwr = float(d['fppwr']) / 1000.0
                rx_pwr = float(d['rxpwr']) / 1000.0
                cifo = float(d['cifo']) / 1000.0  # ppm

                # retrieve coordinates
                init_coords = coordById[init]
                resp_coords = coordById[resp]

                # compute the error
                true_dist = distance(init_coords, resp_coords)
                err = abs(dist - true_dist)
                err_bias = abs(dist_bias - true_dist)

                # print(f"Ranging error [{init}->{resp}] "
                #       f"{err} bias {err_bias} "
                #       f"(dist {dist}/{dist_bias} true dist {true_dist})")

                log_row = [
                    init, resp, seqn, time_ms, 1, dist, dist_bias,
                    err, err_bias, true_dist,
                    fp_pwr, rx_pwr, cifo]
                writer.writerow([init, resp, seqn] + [format(x, '.2f') for x in log_row[3:]])
                continue

            m = regex_no.match(line)
            if m:

                # get dictionary of matched groups
                d = m.groupdict()

                seqn = int(d['seqn'])
                time_ms = int(d['ms'])

                # retrieve IDs of nodes
                init = int(idByAddr[d['init'][:2] + ':' + d['init'][2:]])
                resp = int(idByAddr[d['resp'][:2] + ':' + d['resp'][2:]])

                # retrieve coordinates
                init_coords = coordById[init]
                resp_coords = coordById[resp]

                true_dist = distance(init_coords, resp_coords)

                # print(f"Ranging failed [{init}->{resp}] ")

                log_row = [
                    init, resp, seqn, 0, np.nan, np.nan,
                    np.nan, np.nan, true_dist,
                    np.nan, np.nan, np.nan]
                writer.writerow([init, resp, seqn] + [format(x, '.2f') for x in log_row[3:]])
                continue
            
            m = regex_ts.match(line)
            if m:

                # get dictionary of matched groups
                d = m.groupdict()

                seqn = int(d['seqn'])

                # retrieve IDs of nodes
                init = int(idByAddr[d['init'][:2] + ':' + d['init'][2:]])
                resp = int(idByAddr[d['resp'][:2] + ':' + d['resp'][2:]])

                writer_ts.writerow([
                    init, resp, seqn,
                    int(d['poll_tx_ts']), int(d['poll_rx_ts']),
                    int(d['resp_tx_ts']), int(d['resp_rx_ts']),
                    int(d['ds_final_tx_ts']), int(d['ds_final_rx_ts'])])
                continue

    outfile.close()


def std(x):
    return np.std(x)


def percentile(n):
    def percentile_(x):
        return np.nanpercentile(x, n)
    percentile_.__name__ = f'{n}'
    return percentile_


def analyze(results_file='ranging_results.csv', analysis_file='ranging_analysis.csv',
            ignore_seqn=0, ignore_s=0):
    df = pd.read_csv(results_file)
    df = df[df.seqn >= ignore_seqn]
    df = df[df.time_ms >= ignore_s/1000.0]
    df['count'] = 1

    df = df.groupby(['init', 'resp', 'dist_ground_truth'])
    df = df[['count', 'success', 'dist', 'dist_bias', 'err', 'err_bias',
             'fp_pwr', 'rx_pwr', 'freq_offset']]
    df = df.agg({
        'count': 'sum',
        'success': ['sum', 'mean'],
        'dist': ['mean', 'std',
                 percentile(50),
                 percentile(75),
                 percentile(95),
                 percentile(99)],
        'dist_bias': ['mean', 'std',
                      percentile(50),
                      percentile(75),
                      percentile(95),
                      percentile(99)],
        'err': ['mean', 'std',
                percentile(50),
                percentile(75),
                percentile(95),
                percentile(99)],
        'err_bias': ['mean', 'std',
                     percentile(50),
                     percentile(75),
                     percentile(95),
                     percentile(99)],
        'fp_pwr': ['mean', 'std'],
        'rx_pwr': ['mean', 'std'],
        'freq_offset': ['mean', 'std']
    }).reset_index()
    df.columns = ['_'.join(col).strip(' _') for col in df.columns.values]

    summary_row = []
    for col in df.columns:
        summary_row.append(df[col].mean())
    summary_row[0] = None
    summary_row[1] = None
    # print(summary_row)
    df.loc[len(df)] = summary_row
    df = df.round(6)
    df.to_csv(analysis_file, index=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("log_file", help="The path to ranging logs")
    parser.add_argument("--ignore", "-i",
                        help="How many initial sequence numbers to"
                             "ignore in analysis",
                        default=0)
    parser.add_argument("--ignore_s", "-s",
                        help="How many initial seconds to"
                             "ignore in analysis",
                        default=0)
    args = parser.parse_args(sys.argv[1:])

    # parse and analyze
    results_file = 'ranging_results.csv'
    analysis_file = 'ranging_analysis.csv'
    parse_file(args.log_file, results_file)
    print("Parsing done.")
    analyze(results_file, analysis_file,
            ignore_seqn=int(args.ignore), ignore_s=int(args.ignore_s))
    print("Analysis done.")
