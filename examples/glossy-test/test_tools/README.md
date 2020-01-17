# `simgen.py` - Plan simulations with less pain!

Simgen is program which automates simulations management.
It does so by:

* generating single folder where simulations are gathered,
* generating a folder for each simulation, named after configuration
  parameters related both to Glossy and Glossy-Test settings in addition
  to the id of the initiator. The folder contains:

  1. the copy of the binary file used;
  2. a `project-conf.h` file used by the simulation according to
     parameters configurations defined;
  3. a `glossy_test_sim.json` file, ready to be sent to the testbed, with
     **absolute paths** to the binary and the pfile required (present in the current
     simulation directory).

**NOTE:**

* `simgen.py` deals with invalid scenarios by raising exceptions.
  These are thought to provide valuable insight on the possible errors
  occurring.

  *So be patient and read the last lines of the traceback.*
  *You won't regret it!*


## Simulations settings

Simgen produces simulations based on settings defined in the
`params.py` file.

```python
PARAMS = {
    "app_folder": "../",                                # Path where the glossy_test.c file is placed
    "sims_dir": "../simulations_smarttx_manual",        # Path where to store simulations in

    # list of nodes available
    "nodes" : [11, 12, 10, 13, 14, 15, 16, 17, 2, 1, 4, 5],

    # -------------------------------------------------------------------------
    # COMBINATIONS: the following parameters would produce a simulation
    #               for each combination
    # -------------------------------------------------------------------------
    # list describing, in each simulation, the node to be the initiator.
    # There will be a **separate** simulation for each of these nodes.
    #"initiator": [11, 12, 10, 13, 14, 15, 16, 17, 2, 1, 4, 5],
    "initiator" : [11, 13, 15, 17, 5],

    # "versions"  : ["std_dynamic", "std_static", "txo_dynamic", "txo_static"]
    "versions"  : ["std_static", "txo_static"],        # the two main versions

    # each power configuration is a tuple <smarttx, power> in
    # which smarttx is 0 if disabled and 1 if enabled. Power is
    # a 4 bytes numerical value defining the configuration to be employed
    # wrt transmission power
    "powers"    : [(0, 0x9a9a9a),
                   (1, 0x3a5a7a9a)],
    "payloads"  : [0, 109],
    # -------------------------------------------------------------------------
    # END-COMBINATIONS
    # -------------------------------------------------------------------------
    # log level used: "all", "none", "debug", "info", "error"
    "log_level" : "none",

    "ts_init"  : "asap",
    "duration" : 300                                               # Duration of each simulation
}
```

Where:

* `app_folder` is the path where the `glossy_test.c` is
   supposed to be found,

* `sims_dir` is the path used to store simulations produced,

* `nodes` defines the list of available tesbed nodes,

* `initiator` defines a list of nodes that will be initiators, in
   **different simulations**. At every simulation there will
  be a single initiator.

* `versions` defines the set of versions to be used in
  simulations.

* `powers` defines a list of tuples *<smarttx, txpower>*
  defining the use of the smart tx power feature provided by the dw1000
  and the corresponding tx power.

* `payloads` allows to change the frame length by varying the
  length of the array defined in `glossy_test.c` and included
  within the glossy payload. So it is not the only
  variable involved to determine the total frame length, but
  it is what largely affects it.

* `duration` defines the duration (in **seconds**) for a single
  simulation.

* `ts_init` defines the corresponding parameter for the testbed `.json`
  file, with just one extensions:

  in case it defines an actual schedule, in the format `%Y-%m-%d %H:%M`,
  then the first simulation is scheduled at that time and all
  consecutive simulations will be interleaved by an offset equals
  to the duration of a single simulation.


## Usage

By issuing the program `simgen.py` without any argument,
the directory, where simulations will be stored, is made
and a separate folder is made for each initiator defined in `params.py`.

A folder's name differs based on the following:

* it will contain a sequence of three timing values, expressed
  in *milliseconds* represing the *period*, the *slot* and
  the *guard time* used by Glossy-Test;

* it will contain a string `ntx<K>` where `K` defines the maximum number
  of transmissions (within a flood);

* it will contain either the string `std` or `txo` based on
  the Glossy version used, *Standard* or *TX only*, respectively;

* it will contain either the string `dynamic` or `static` based
  on the approach used for slot estimation;

* it will contain a string `smarttx<T>` where `T` is 0
  if the Smart Tx feature is disabled, otherwise 1;

* it will contain a string `txpower0x<B>` where `B` denotes
  a 4bytes numerical value describing the transmission power
  settings employed by the radio.

* it will contain a string `payload<DIM>` where `DIM` is
  the size in byte of the variable length array embedded
  within the glossy frame;

* finally it will contain a string identifying the duration (in
  *seconds*) of the simulation and the id of the initiator.

For instance, one such string is:
`500ms_100ms_1ms_ntx2_txo_static_smarttx0_payload109_duration600_init15`.

By default, during simulation's creation, the process will raise
an error and stops whenever a simulation folder with the same
name already exists.

To avoid this behaviour and forcing the generation process, it is
possible to issue `simgen.py` with the `--force-generation` flag on.
This makes the program **overwrite** the folder in case of name clashes,
**overwriting any possible content**.

**Note:** the simgen automatically produces a `project-conf.h` file
for each simulation. During this process any file with the same name, placed
in the application directory, would be overwritten.
The simgen will therefore throw an exception in case such file is present
at the start of the generation process, suggesting to rename
that file in order to preserve it.

To print the values defined in `params.py` and currently used
by simgen, issue the command `simgen.py -pi`.

To delete the main simulation folder and all its content (hence,
**every simulation**), issue the `simgen.py -d` command.
The program will ask the user to enter a randomly generated 5 cyphers
code and, upon correct insertion, will delete the main simulation
folder.

**Remember:** `simgen.py -h` is your friend!

## A practical use case

1. Write the `params.py` file and define relative paths to
   the `glossy_test.c` file and the destination directory;
   define the duration of simulations using `ts_init`,
   the set of testbed nodes to use and the set
   of testbed nodes to be use as initiators.

2. Execute `simgen.py` (`-fg` if you are sure to overwrite possible
   folders already existing).

3. Browse each new simulation's folder and schedule the simulation
   to the testbed.


