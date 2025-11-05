# Example of params.py file
PARAMS = {
    "app_dir": "../",                     # Path where the main .c file is
    "sims_dir": "./test_simulations",        # Path where to store simulations in

    # list of nodes available
    "nodes" : [ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
               11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
               21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
               31, 32, 33, 34, 35, 36],

    "sinks": [1],   # single sink per simulation

    "num_originators": [1],
    "epochs_per_cycle": 5,

    "originators_schedule": [[1],[2],[3],[4],[5]],              # optional

    # "max_slot": 50,                                           # parameter removed
    "start_epoch": 2,

    "seed": 123,

    "log_level": "error",

    "start_time": "asap",
    "duration": 60                                               # Duration of each simulation
}
