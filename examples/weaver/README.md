# Weaver

This folder contains the implementation of Weaver, a collection protocol leveraging concurrent transmissions (CTX).


## Code structure

We implement Weaver for the Decawave DW1000 radio, targeting the EVB1000 board. The code targets Contiki OS.

Weaver exploits TSM to build a synchronised flood where the sink is the time reference. The code for TSM can be found at [contiki-uwb/dev/dw1000/tsm/](../../dev/dw1000/tsm).

Energy information is managed by the Statetime module, which is embedded in TSM. The source code of Statetime can be found in [contiki-uwb/dev/dw1000/dw1000-statetime.c](../../dev/dw1000/dw1000-statetime.c). More information on the Statetime module can be found in
the [README](../../dev/dw1000/statetime.md).


Weaver maps the set of deployed node to a bitmap in which each node is assigned a specific index. At the moment the bitmap is 64bit long. It is possible to configure the deployed nodes creating a specific folder for your topology and defining the node addresses in [contiki-uwb/examples/deployment](../deployment). For a complete explanation of the required steps, please consult the [README](../deployment/README.md).

Weaver code is structured as follows:

* `weaver.c` contains the Weaver logic and an application that schedules *U* initiators at every epoch;

* `weaver-utility` contains operations concerning the bitmap used by Weaver in local and global acknwoledgemnts;

* `weaver-log` provides convenient functions to collect and print information to be logged at the end of the epoch;

* `rrtable` is responsible to define a pre-allocated bounded FIFO queue used to store incoming data.

## Running experiments

We provide scripts to conveniently build Weaver from a configuration file that allows to define the sink node, the payload to be used and several other parameters.

An example of such a configuration file can be found in `experiments/example/params.py`. Weaver can be built executing the `builder.py` script in the same folder of the configuration file:

```
../../test_tools/builder.py
```

The result will be created in the folder specified in the configuration file. When processing logs, we assume that run logs are within the same folder.

## Processing scripts

Test logs are processed with the `log_parser2.py` script as follow:

```
test_tools/log_parser2.py test.log
```

A `stats` folder is created within the log file parent directory, containing individual `csv` files related to the perfomance of the protocol and the statistics used for further processing. The `summary.csv` file provides the performance of the protocol at each epoch, as well as its mean performance (the very last row).

## Publications
The following work was published in ["One Flood to Route Them All: Ultra-fast Convergecast of Concurrent Flows over UWB"](https://dl.acm.org/doi/10.1145/3384419.3430715):
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
keywords = {low-power wireless, ultra-wideband, concurrent transmissions},
location = {Virtual Event, Japan},
series = {SenSys '20}
}
```
