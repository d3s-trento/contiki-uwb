PARAMS = {
    "app_dir": "../..",                 # Path where the weaver.c file is
    "sims_dir": "./weaver-sink19",      # Path used to store simulations

    # list of nodes available
    "nodes" : [ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
               11, 12, 13, 14, 15, 16, 17, 18, 19, 
                       23, 24, 25, 26, 27, 28, 29, 30,
               31, 32, 33, 34, 35, 36],

    # <sink_id, sink_radius>
    "sinks": [(19, 7)],                 # single sink per simulation

    "num_originators": [0, 1, 5, 10, 20, 30],
    "epochs_per_cycle": 100,            # number of different originator combinations

    "start_epoch": 20,                  # the first epoch where all U originators will start
                                        # disseminating their data

    "seed": 123,
    "payload": 0,                       # 0 = 23B frame

    # Testbed run paramenters
    "start_time": "asap",
    "duration": 300
}
