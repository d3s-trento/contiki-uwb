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
`RANGING_TIME` is set based on the value of `RANGING_STYLE`. However, adjustments may be needed for extreme radio configurations.

The constant `ROUND_PERIOD` sets the total ranging round period. It is checked at compile time that
all the rangings fit inside that time.

The constants `ACQUIRE_CIR`, `CIR_START_FROM_PEAK` and `CIR_MAX_SAMPLES` control the CIR acquisition.
Note that CIR printing might be long, so the time allocated for a single ranging increases significantly.
The pre-defined value of `MAX_PRINTING_DELAY` is set large enough to print the full CIR. If only part is printed, that value may be adjusted (reduced).

The constant `PRINT_RXDIAG` enables/disables printing the RX diagnostics for the last ranging packet received.

## Radio configuration

You can modify the radio configuration in `project-conf.h`.

```
#define DW1000_CONF_CHANNEL        5                // frequency channel
#define DW1000_CONF_PRF            DWT_PRF_64M      // pulse repetition frequency (16M or 64M, affects preamble)
#define DW1000_CONF_PLEN           DWT_PLEN_128     // preamble length in symbols (roughly 1us per symbol)
#define DW1000_CONF_PAC            DWT_PAC8         // preamble accumulation count (length of symbol chunks for preamble detection)
#define DW1000_CONF_SFD_MODE       0                // standard (0) or manufacturer-defined (1) start-of-frame delimiter
#define DW1000_CONF_DATA_RATE      DWT_BR_6M8       // data rate (one of DWT_BR_110K, DWT_BR_850K or DWT_BR_6M8)
#define DW1000_CONF_PHR_MODE       DWT_PHRMODE_STD  // PHY header mode, use DWT_PHRMODE_EXT for extended frames (payload > 127B)
#define DW1000_CONF_PREAMBLE_CODE  9                // preamble code (code choice is constrained by channel and PRF)
#define DW1000_CONF_SFD_TIMEOUT    (129 + 8 - 8)    // SFD timeout (in symbols); set enough symbols for the preamble and the SFD, PAC can be subtracted
```

Refer to [Qorvo documents](https://www.qorvo.com/products/p/DW1000#documents), and the software API guide in particular, for more information.

### Breaking configuration warning

Multi-ranging uses the `range_with()` function offered by the DW1000 port (see `dev/dw1000/dw1000-ranging.c`).
The port automatically sets several timeouts when the function is called, but it is possible to configure the radio in a way that breaks the assumptions of the DW1000 port.

## Important notes

Make sure that all tags share the same configuration (same firmware). Anchor nodes need to be reflashed
only if the radio configuration changes, but may keep the firmware if only the set of tags changes.

Pay attention to error messages in the logs. If those appear, multiple rangings might overlap and fail or provide incorrect results.

If you enable CIR printing, make sure that the device is connected to a computer and the USB output is actually read.
Otherwise, the device will reboot due to watchdog.
