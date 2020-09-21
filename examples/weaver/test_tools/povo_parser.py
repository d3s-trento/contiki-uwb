#!/usr/bin/python3
import re
import logging

from collections import OrderedDict

logger = logging.getLogger(__name__)
logging.basicConfig(level=logging.DEBUG)
logging.getLogger(__name__).setLevel(level=logging.DEBUG)

# -----------------------------------------------------------------------------
# REGEX
# -----------------------------------------------------------------------------
TESTBED_POVO_PREFIX = r"(?i)\[\d+-\d+-\d+\s+\d+:\d+:\d+,\d+\]" +\
        "\s+[a-zA-Z0-9:\-_\.]+\s*"
TESTBED_POVO_PREFIX_GENERIC = TESTBED_POVO_PREFIX + r"\s+(.*)"
TESTBED_POVO_PREFIX_DEVICE_OUT = TESTBED_POVO_PREFIX + r"\s+(\d+)\.\w+\s*<\s*(.*)"
TESTBED_POVO_PREFIX_DEVICE_IN = TESTBED_POVO_PREFIX + r"\s+(\d+).\w+\s*>\s*(.*)"
# -----------------------------------------------------------------------------
TESTBED_POVO_END_TEST = r"(?i).*end\s+test"
TESTBED_POVO_ACTIVE_NODES = r"(?i).*activenodes:\s+(?P<n_active_nodes>\d+)\s+\[(?P<node_list>.*)\]"
# -----------------------------------------------------------------------------

FILTER_PREFIX = "prefix"
FILTER_DOUT   = "device-out"
FILTER_DIN    = "device-in"
FILTER_OTHER  = "other"

CONTENT_END    = "end"
CONTENT_ACTIVE = "active-nodes"
CONTENT_OTHER  = "other"


# define rules with the corresponding label.
# be careful to define longer rules first!
FILTER_RULES = OrderedDict([
    (TESTBED_POVO_PREFIX_DEVICE_OUT, FILTER_DOUT),
    (TESTBED_POVO_PREFIX_DEVICE_IN, FILTER_DIN),
    (TESTBED_POVO_PREFIX_GENERIC, FILTER_PREFIX)
])

TESTBED_CONTENT = OrderedDict([
    (TESTBED_POVO_END_TEST, CONTENT_END),
    (TESTBED_POVO_ACTIVE_NODES, CONTENT_ACTIVE),
    (r"(.*)", CONTENT_OTHER)
])

def match_filter(log, rules, compiled=False):
    """
    Return None if there is no rule in rules matching
    the log.

    Return the log if it matches a rule or None.
    """
    if not compiled:
        rules = {re.compile(r): label for r, label in rules.items()}

    for rule in rules:
        match = rule.match(log)
        if match:
            return rules[rule], match
    return None, None

def convert_log_content(log):
    """Remove special characters and remove bytestring notation"""
    if log[0] == "b":
        # return string without bytecode repr -> b''
        log = log[2:-1]
    # replace special characters with a single blank space
    log = re.sub(r"(?:\\t|\\n)+", " ", log)
    return log

def parse_node_logs(filepath, info={}):
    """
    Return <node_id, log> pairs from the sequence
    of logs contained in the given file.

    In the info optional dictionary additional information
    provided by the testbed are provided.
    """
    with open(filepath, "r") as fh:
        line_cnt = 0
        ended = False
        for line in fh:
            line_cnt += 1

            node_id = None
            log = ""
            rule, match = match_filter(line, FILTER_RULES)

            # no matching rule
            if rule is None:
                logger.debug("Log doesn't match any testbed pattern!")
                continue

            if rule == FILTER_PREFIX:
                log = match.group(1)
            elif rule in (FILTER_DIN, FILTER_DOUT):
                node_id, log = match.groups()
            else:
                raise ValueError("Invalid prefix found at line %d: %s" % (line_cnt,line))

            log = convert_log_content(log)
            label, match = match_filter(log, TESTBED_CONTENT)

            if label == CONTENT_END:
                info[CONTENT_END] = line_cnt
                logger.info("End of test reached at line %d" % line_cnt)
                ended = True
            elif label == CONTENT_ACTIVE:
                n_nodes, active_nodes = match.groups()
                active_nodes = [node.strip() for node in active_nodes.split(",")]
                info[CONTENT_ACTIVE] = active_nodes
                logger.info("Active nodes found")
            else:
                # the log is not an internal one, if it is related
                # to a node, then return it.
                if not ended and node_id is not None:
                    yield node_id, match.group(1)


if __name__ == "__main__":
    import argparse
    # -------------------------------------------------------------------------
    # PARSING ARGUMENTS
    # -------------------------------------------------------------------------
    parser = argparse.ArgumentParser()
    # required arguments
    parser.add_argument("log_file",\
            help="The log file to parse")
    args = parser.parse_args()

    info = {}
    for node_id, log in parse_node_logs(args.log_file, info):
        print("%s ---> %s" % (node_id, log))
    print("testbed info: %s" % str(info))
