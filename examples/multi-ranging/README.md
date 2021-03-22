# Multi-tag ranging application

This application allows performing periodic ranging between a set of tags and a
set of anchors in round-robin. It declares one of the tags a master that
commands other tags when to perform ranging, to avoid ranging collisions.

This works under the assumption that the tags are within the communication
range with the master tag. If a tag leaves the communication range or
misses the command (which may happen sporadically), it skips the ranging round(s).


## Configuration

The configuration is set at compile time.

Radio configuration is defined in project-conf.h. It is recommended to use the
default configuration (Ch. 5, PRF64, preamble 128, 6.8 Mbps).

The application itself is configured in multi-rng.c file. The first necessary
thing to do is to set device addresses according to their roles. Addresses are
specified as `{{A0, A1}}` for short-address configuration or `{{A0, A1, A2, A3, A4, A5, A6, A7}}` 
for long addresses. The values should be set in the same order as they are printed on the
LCD screen of EVB1000 or output to the serial line for DWM1001 on device boot.
E.g. if the screen shows `ABCD`, the address should be written `{{0xAB, 0xCD}}`.

There should be set one address for the master tag, optionally one or more other tags,
and at least one anchor. Tag addresses (including the master) might be listed in the
anchor list, in this case tags will also range among themselves.

The application may use one of the two ranging methods: Single-Sided Two-Way Ranging (SS-TWR) or
Double-Sided Two-Way Ranging (DS-TWR), defined by the `RANGING_STYLE` constant. 
SS-TWR is recommended, it provides very similar accuracy but uses only 2 messages instead of 4,
therefore it is faster and less affected by packet loss.

The constant `ROUND_PERIOD` sets the total ranging round period. It is checked at compile time that
all the rangings fit inside that time.

The constants `ACQUIRE_CIR`, `CIR_START_FROM_PEAK` and `CIR_MAX_SAMPLES` control the CIR acquisition.
Note that CIR printing might be long, so the time allocated for a single ranging increases significantly.
The pre-defined value of `MAX_PRINTING_DELAY` is set large enough to print the full CIR. If only part is printed, that value may be adjusted (reduced).

The constant `PRINT_RXDIAG` enables/disables printing the RX diagnostics for the last ranging packet received.

## Important note
Make sure that all tags share the same configuration (same firmware). Anchor nodes need to be reflashed
only if the radio configuration changes, but may keep the firmware if only the set of tags changes.

Pay attention to error messages in the logs. If those appear, multiple rangings might overlap and fail or provide incorrect results.
