PARAMS = {
    "app_dir": "../..",                     # Path where the main .c file is
    "sims_dir": "./weaver-sink19",       # Path where to store simulations in

    # list of nodes available
    "nodes" : [ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
               11, 12, 13, 14, 15, 16, 17, 18, 19, 
                       23, 24, 25, 26, 27, 28, 29, 30,
               31, 32, 33, 34, 35, 36],

    # <sink_id, sink_radius>
    "sinks": [(19, 7)],   # single sink per simulation

    "num_originators": [30],
    "epochs_per_cycle": 100,

    "start_epoch": 20,

    "seed": 123,
    "payload": 0,   # 23B frame

    "start_time": "asap",
    "duration": 300
}
