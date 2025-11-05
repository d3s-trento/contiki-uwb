# Synchronized Action Manager for Ultra-Wideband (SAMU)

## Overview

SAMU is a framework for building time-slotted and time-synchronized communication protocols. By handling all scheduling operations autonomously, it allows the programmer to focus on protocol logic rather than the intricacies of the hardware and the RF medium.

### Concurrent transmissions, and more

SAMU was developed with the goal to support concurrent transmissions (CTX, also known as synchronous transmissions, STX). CTX is a communication technique based on packets overlapping non-destructively at receivers, which has been applied in communication protocols to unlock substantial improvements in realiability and energy efficiency compared to traditional approaches. Yet, SAMU can be used beyond CTX to support any time-slotted schedule.  

### Prominent features

- Sequential programming style seamlessly combining fine-grained control and high-level protocols
- Easy-to-use synchronization between nodes, even across deep sleep intervals
- Configurable action duration at runtime
- Automatic logging of the operations within each epoch (with a viewer, called [Inspector](./tools/inspector/README.md), to visualize them)
- Preamble and frame timeouts to improve the RX energy efficiency when there is no data to receive
- Energy consumption estimation through [StateTime](dev/dw1000/statetime.md)

Although the current implementation targets the Qorvo DW1000 radio on the EVB1000 platform, the code herein can be ported to other platforms with minimal effort making the code written on top of SAMU platform-independent.

### Contiki for UWB radios

SAMU is implemented for the EVB1000 platform atop our UWB port of Contiki OS.
You can find more information on the [main branch](https://github.com/d3s-trento/contiki-uwb/tree/master) of this repository.

## Code Structure

The core of SAMU can be found in [samu](./samu).

```
├── cpu
│   └── stm32f105
├── dev
|   └── dw1000
|       ├── decadriver
|       ├── dw1000-statetime-evb1000.c
|       ├── dw1000-statetime-generic.c
|       ├── dw1000-statetime.h
|       ├── evb1000-timer-mapping.c
|       ├── evb1000-timer-mapping.h
|       └── samu-radio-defaults.h
├── samu
│   ├── ascii85.c
│   ├── ascii85.h
│   ├── contiki-samu.c
│   ├── contiki-samu.h
│   ├── modules
│   │   ├── crystal
│   │   ├── glossy
│   │   └── glossy_example
│   ├── rtimer-drift.c
│   ├── rtimer-drift.h
│   ├── samu.c
│   ├── samu.h
│   ├── samu-logs.c
│   ├── samu-logs.h
│   ├── samu-ral.c
│   ├── samu-ral.h
│   ├── samu-ral-status.h
│   ├── tools
│   │   └── inspector
│   ├── uwb-to-rtimer.c
│   └── uwb-to-rtimer.h
├── systems
│   ├── deployment
│   ├── crystal-app
│   ├── sync-app
│   └── weaver
└── platform
    └── evb1000
```

## Systems

This repository includes the implementations of popular CTX protocols.

* [Crystal](https://dl.acm.org/doi/10.1145/2994551.2994558), a fast and reliable data collection protocol based on Glossy ([systems/crystal-app/](./systems/crystal-app), exploits the Glossy and Crystal modules of SAMU)
* [Weaver](https://dl.acm.org/doi/10.1145/3384419.3430715), a next generation data collection protocol based on concurrent transmissions ([systems/weaver/](./systems/weaver), with optional Flick integration) [video](http://disi.unitn.it/~picco/papers/sensys20_weaver.mp4)
* You can find more informations and examples on [Flick](https://dl.acm.org/doi/10.1145/3583120.3586967), a primitive for fast network-wide decisions, in a [dedicated branch](https://github.com/d3s-trento/contiki-uwb/tree/flick)

To compile one of the applications above, first apply the correct compilation options specified below. Then, follow the general instructions on the [main branch](https://github.com/d3s-trento/contiki-uwb/tree/master) for building and flashing your program.

These applications have been evaluated on the [Cloves](https://iottestbed.disi.unitn.it/cloves/) public testbed located at the University of Trento. 

## Compilation Options for SAMU

To enable SAMU, define the following in your application Makefile:
```
UWB_WITH_SAMU = 1
```
This will exclude all other Contiki stacks.

To enable the SAMU modules that directly provide ready-to-use protocol implementations,
define the following in your application Makefile:
```
UWB_WITH_SAMU_GLOSSY = 1
UWB_WITH_SAMU_CRYSTAL = 1
```

Note that our Crystal application requires the Glossy and Crystal modules. Weaver does not use Glossy and therefore needs no extra modules.

## Using SAMU

SAMU abstracts the low-level details of communicating with the radio.
Yet, when needed, SAMU enables fine-grained control of single TX/RX *actions*, which consitute the building blocks of a SAMU schedule.

### Actions
SAMU supports the following actions:
- `SAMU_SCAN` starts a reception slot with no maximum listening time
- `SAMU_RX` starts a reception slot with a maximum listening time due to preamble timeout or frame reception timeout
- `SAMU_TX` transmits the specified message
- `SAMU_FLICK` initiates or propagates a Flick flood
- `SAMU_RESTART` puts the radio to sleep and automatically restarts at the beginning of the next epoch while maintaining synchronization between nodes

### Silvers
The "time" of a SAMU schedule is expressed in _slivers_. They determine when the next SAMU action (RX, TX, ...) will be performed, are included in the SAMU header, and can be used by a receiver to synchronize to the transmitter. An actions has a certain duration in slivers, which can be adjusted on a per-action fashion for maximum flexibility.

### Examples
Reception example: 
```c
SAMU_RX(&pt, buffer);

if (SAMU_PREV_A.status == RX_SUCCESS) {
  // Correct reception
} else if (SAMU_PREV_A.status == RX_ERROR) {
  // Received invalid packet (e.g., wrong CRC, wrong Reed Solomon, wrong PHR SECDED, SFD Timeout, ...)
} else if (SAMU_PREV_A.status == RX_TIMEOUT) {
  // Didn't receive anything
}
```

Bootstrap and synchronization example[^1]: 
```c
while (1) {
  SAMU_SCAN(&pt, buffer);
  
  if ((SAMU_PREV_A.status == RX_SUCCESS) && (SAMU_PREV_A.payload_len == sizeof(pkt_t))) {
    memcpy(&rcvd, buffer + SAMU_HDR_LEN, sizeof(pkt_t));
    
    if (extra_application_checks()) {
      // Synchronize using the received packet
      SAMU_NEXT_A.accept_sync = true;
      
      // Exit the bootstrap loop
      break;
    }
  }
}
```

Transmission example: 
```c
memcpy(buffer+SAMU_HDR_LEN, &node_pkt, sizeof(info_t));
SAMU_TX(&pt, buffer, sizeof(info_t));
```

See [systems](./systems) for fully-fledged implementations.


## Publications
- TSM, the predecessor of SAMU, was presented and used in [Weaver](https://dl.acm.org/doi/10.1145/3384419.3430715)
```
@inproceedings{10.1145/3384419.3430715,
	author = {Trobinger, Matteo and Vecchia, Davide and Lobba, Diego and Istomin, Timofei and Picco, Gian Pietro},
	title = {One Flood to Route Them All: Ultra-Fast Convergecast of Concurrent Flows over UWB},
	year = {2020},
	isbn = {9781450375900},
	publisher = {Association for Computing Machinery},
	address = {New York, NY, USA},
	url = {https://doi.org/10.1145/3384419.3430715},
	doi = {10.1145/3384419.3430715},
	abstract = {Concurrent transmissions (CTX) enable low latency, high reliability, and energy efficiency. Nevertheless, existing protocols typically exploit CTX via the Glossy system, whose fixed-length network-wide floods are entirely dedicated to disseminating a single packet.In contrast, the system we present here, Weaver, enables concurrent dissemination towards a receiver of different packets from multiple senders in a single, self-terminating, network-wide flood.The protocol is generally applicable to any radio supporting CTX; the prototype targets ultra-wideband (UWB), for which a reference network stack is largely missing. Our modular design separates the low-level mechanics of CTX from their higher-level orchestration in Weaver. Other researchers can easily experiment with alternate designs via our open-source implementation, which includes a reusable component estimating UWB energy consumption.Our analytical model and testbed experiments confirm that Weaver disseminates concurrent flows significantly faster and more efficiently than state-of-the-art Glossy-based protocols while achieving higher reliability and resilience to topology changes.},
	booktitle = {Proceedings of the 18th Conference on Embedded Networked Sensor Systems},
	pages = {179–191},
	numpages = {13},
	keywords = {ultra-wideband, concurrent transmissions, low-power wireless},
	location = {Virtual Event, Japan},
	series = {SenSys '20}
}
```
- Used and modified in ["Network On or Off? Instant Global Binary Decisions over UWB with Flick"](https://dl.acm.org/doi/10.1145/3583120.3586967)
```
@inproceedings{soprana2023network,
  title = {Network On or Off? Instant Global Binary Decisions over UWB with Flick},
  author = {Soprana, Enrico and Trobinger, Matteo and Vecchia, Davide and Picco, Gian Pietro},
  booktitle = {Proceedings of the 22nd International Conference on Information Processing in Sensor Networks},
  url = {https://dl.acm.org/doi/10.1145/3583120.3586967},
  doi = {10.1145/3583120.3586967},
  pages = {261--273},
  year = {2023}
}
```

- SAMU is based on many of the principles of TSM, but introduces action APIs, dynamic action duration (with "slivers"), deep sleep support, and more.
```
...
```

## Disclaimer
Although we tested the code extensively, it is considered a research prototype that likely contains bugs. We take no responsibility for and give no warranties in respect of using this code.
