#!/usr/bin/env python3

import sys

def v2db(v):
    if v < 0 or v > 255:
        return None
    coarse = v >> 5
    fine   = v & 0b00011111
    return 15 - coarse*2.5 + fine*0.5


if len(sys.argv) < 2:
    print(sys.argv[0], "hex_value")
    sys.exit(0)

v = int(sys.argv[1], 16)

gain = v2db(v)

print(f"Value: {v}, output gain: {gain} dB")


