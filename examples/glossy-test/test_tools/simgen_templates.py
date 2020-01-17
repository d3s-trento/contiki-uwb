SERIALCOM_TEMPLATE = \
r"""
# THIS FILE HAS BEEN AUTOMATICALLY GENERATED!

import testbed.Testbed
import logging
import time

logger = logging.getLogger(__name__)

def run_test(testbed):
    logger.info("Script begins")

    init_id = {{ init_id }}
    while True:
        for n in testbed.activeNode:
            n.flush()
            binstring = bytes("INITIATOR {}\n".format(init_id), "ascii")
            n.write(binstring)

        # wait before announcing the initiator again
        time.sleep(2)

    logger.info("Script ends")

"""

TESTBED_TEMPLATE = \
r"""
{
    "name" : "Glossy Test",
    "description" : "Run Glossy for {{ duration_minutes }} minutes",
    "ts_init" : "{{ ts_init }}",
    "duration" : {{ duration_seconds }},
    "image" : {
        "hardware" : "evb1000",
        "file":    "< path-to-binary >",
        "target":  "< list-of-targets >"
    },
    "logs" : 0
}
"""
