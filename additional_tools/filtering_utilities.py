import argparse

import pandas as pd

def parse_int_or_none(a: str) -> int|None:
    a = a.strip()

    if len(a) == 0:
        return None

    try:
        a = int(a)

        return a
    except ValueError:
        raise argparse.ArgumentError('Should be an integer or empty')


def parse_int(a: str, default: int):
    val = parse_int_or_none(a)

    if val is None:
        return default

    return val


def add_numNodes(parser: argparse.ArgumentParser, numNodes_default: int):
    if parser._option_string_actions.get('--numNodes') is None:
        parser.add_argument('--numNodes', dest='numNodes', default=numNodes_default, type=int, help='Number of nodes (excluding sink)')
    elif parser._option_string_actions['--numNodes'].default != numNodes_default:
        raise Exception('Two different defaults for numNodes requested')


def add_epoch_filter(parser: argparse.ArgumentParser, enabled_default: bool, min_default: int, max_default: int):
    def epoch_filter_config(a: str) -> tuple[bool, int, int]:
        _min, _max = a.split(',', 2)

        return True, parse_int(_min, min_default), parse_int(_max, max_default)


    parser.add_argument('--no-epochFilter',
                        '-nef',
                        dest='epochFilter',
                        default=(enabled_default, None if not enabled_default else min_default, None if not enabled_default else max_default),
                        action='store_const',
                        const=(False, None, None),
                        help='filter by epoch')

    parser.add_argument('--epochFilter',
                        '-ef',
                        dest='epochFilter',
                        nargs='?',
                        const=(True, min_default, max_default),
                        type=epoch_filter_config,
                        help='filter by epoch')


def add_synch_filter(parser: argparse.ArgumentParser, enabled_default: bool, numNodes_default: int):
    parser.add_argument('--no-synchFilter',
                        '-nsf',
                        dest='synchFilter',
                        default=enabled_default,
                        action='store_false',
                        help='filter out epochs in which not all the nodes received the bootstrap')

    parser.add_argument('--synchFilter',
                        '-sf',
                        dest='synchFilter',
                        default=enabled_default,
                        action='store_true',
                        help='filter out epochs in which not all the nodes received the bootstrap')

    add_numNodes(parser, numNodes_default)


def add_completeEpoch_filter(parser: argparse.ArgumentParser, enabled_default: bool, numNodes_default: int):
    parser.add_argument('--no-completeEpochFilter',
                        '-ncef',
                        dest='completeEpochFilter',
                        default=True,
                        action='store_false',
                        help='filter out epochs in which not all the nodes completed the epoch')

    parser.add_argument('--completeEpochFilter',
                        '-cef',
                        dest='completeEpochFilter',
                        default=True,
                        action='store_true',
                        help='filter out epochs in which not all the nodes completed the epoch')

    add_numNodes(parser, numNodes_default)


def apply_epoch_filter(args: argparse.Namespace, *data: pd.DataFrame): # TODO: Type hinting
    enabled, _min, _max = args.epochFilter

    if not enabled:
        return data

    if _min < 0:
        _min = max(dd['epoch'].max() for dd in list(data))+_min

    if _max < 0:
        _max = max(dd['epoch'].max() for dd in list(data))+_max

    if len(data) == 1:
        return raw_apply_epoch_filter(data[0], _min, _max)
    else:
        return tuple([raw_apply_epoch_filter(dd, _min, _max) for dd in data])


def raw_apply_epoch_filter(data: pd.DataFrame, _min: int, _max: int) -> pd.DataFrame:
    return data[(data['epoch'] > _min) & (data['epoch'] < _max)]


def get_complete_epochs(epochData: pd.DataFrame, num_of_nodes: int):
    #TODO: Check
    w = epochData.groupby('epoch').nunique('node_id').reset_index()
    return set(w[w['node_id'] == num_of_nodes]['epoch'])


def to_range(a: set):
    a = sorted(a)

    if len(a) < 2:
        return a

    res = []

    if a[0]+1 == a[1]:
        prevVal = a[0]
    else:
        prevVal = None
        res.append(a[0])

    for i in range(1, len(a)):
        if a[i] == a[i-1]+1:
            if prevVal is None:
                prevVal = a[i]
        else:
            if prevVal is None:
                res.append(a[i])
            else:
                res.append((prevVal, a[i]))

            prevVal = None

    if prevVal is not None:
        res.append((prevVal, a[-1]))

    return res


def ranges_to_str(a):
    return ', '.join(f'{v[0]}-{v[1]}' if isinstance(v, tuple) else f'{v}' for v in a)

def pp_epochs(a):
    return '<' + ranges_to_str(to_range(a)) + '>'
