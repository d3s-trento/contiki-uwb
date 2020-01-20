# Example of params.py file
PARAMS = {
    "app_folder": "../..",                # Path where the glossy_test.c file is
    "sims_dir": ".",                      # Path where to store the test files

    # list of nodes available
    "nodes" : [1, 2, 3],

    # -------------------------------------------------------------------------
    # COMBINATIONS: the following parameters would produce a simulation
    #               for each combination
    # -------------------------------------------------------------------------
    # list describing, in each simulation, the node to be the initiator.
    # There will be a **separate** simulation for each of these nodes.
    "initiator": [1],

    # "versions"  : ["std_dynamic", "std_static", "txo_dynamic", "txo_static"]
    #"versions"  : ["txo_static", "std_dynamic"],
    "versions"  : ["std_static"],

    # each power configuration is a tuple <smarttx, power> in
    # which smarttx is 0 if disabled and 1 if enabled. Power is
    # a 4 bytes numerical value defining the configuration to be employed
    # wrt transmission power
    "powers"    : [(0, 0x9a9a9a9a)],
    "ntxs"       : [2],
    "payloads"  : [3],
    #"payloads"  : [115],
    # -------------------------------------------------------------------------
    # END-COMBINATIONS
    # -------------------------------------------------------------------------
    # log level used: "all", "none", "debug", "info", "error"
    "log_level" : "error",

    # If a time is given in format %Y-%m-%d %H:%M
    # then the first simulation is scheduled at that time
    # and all consecutive simulations will be scheduled
    # after an offset equals to the test duration
    # Time to schedule the simulation.
    "ts_init"  : "asap",
    "duration" : 240                                # Duration of each test
}
