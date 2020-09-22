# Weaver

This folder contains the implementation of Weaver, a collection protocol
leveraging CTX.


## Code structure

We implement Weaver for the Decawave DW1000 radio, targeting
the EVB1000 board. The code targets Contiki OS.

Weaver exploits TSM to build a synchronised flood where the sink
is the time reference.  
The code for TSM can be found at [contiki-uwb/dev/dw1000/tsm/](../../dev/dw1000/tsm).

Energy information is managed by the Statetime module, which
is embedded in TSM. The source code of Statetime can be
found in [contiki-uwb/dev/dw1000/dw1000-statetime.c](../../dev/dw1000/dw1000-statetime.c).  
More information on the Statetime module can be found in
the [README](../../dev/dw1000/statetime.md).


Weaver maps the set of deployed node to a bitmap in which each node
is assigned a specific index. At the moment the bitmap is 64bit long.  
It is possible to configure the deployed nodes creating a specific
folder for your topology and defining the node addresses in
[contiki-uwb/examples/deployment](../deployment). For a complete
explanation of the required steps, please consult the [README](../deployment/README.md).

Weaver code is structured as follows:

* `weaver.c` contains the Weaver logic and an application
  that schedules *U* initiators at every epoch;

* `weaver-utility` contains operations concerning the bitmap
  used by Weaver in local and global acknwoledgemnts;

* `weaver-log` provides convenient functions to collect and print
  information to be logged at the end of the epoch;

* `rrtable` is responsible to define a pre-allocated bounded
  FIFO queue used to store incoming data.

## Running experiments

We provide scripts to conveniently build Weaver
from a configuration file that allows to define
the sink node, the payload to be used and several
other parameters.

An example of such a configuration file can be found
in `experiments/example/params.py`. Simulations
can be built executing the `builder.py` script
in the same folder of the configuration file:

```
../../test_tools/builder.py
```

Simulations will be created in the folder specified
in the configuration file. When processing logs,
we assume that run logs are within the corresponding
simulation folder.

## Processing scripts

Test logs are processed with the `log_parser2.py` script
as follow:

```
test_tools/log_parser2.py test.log
```

A `stats` folder is created within the log file parent
directory, containing individual `csv` files related
to the perfomance of the protocol and the statistics used
for further processing. The `summary.csv` file provides
the performance of the protocol at each epoch, as well as
its mean performance (the very last row).

