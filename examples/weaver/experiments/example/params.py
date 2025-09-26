PARAMS = {
    "app_dir": "../..",                 # Path where the weaver.c file is
    "sims_dir": "./weaver-sink19",      # Path used to store simulations

    # list of nodes available
    # "nodes" : [ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
    #            11, 12, 13, 14, 15, 16, 17, 18, 19, 
    #                    23, 24, 25, 26, 27, 28, 29, 30,
    #            31, 32, 33, 34, 35, 36],
    "nodes" : [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
               19, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 100, 101,
               102, 103, 104, 105, 106, 107, 109, 110, 111, 152, 153, 150, 151,
               149, 148, 147, 138, 137, 136, 135, 134, 133, 131, 132, 128, 127,
               130, 129, 125, 126, 124, 123, 122, 121, 118, 119, 145, 146, 143,
               144, 141, 142, 139, 140],

    # <sink_id, sink_radius>
    "sinks": [(119, 13)],                 # single sink per simulation

    #"random_originators": [ 1,  2,  3,  4,  5,  6,  7,  8,  9, 10,
    #                             11, 12, 13, 14, 15, 16, 17, 18,
    #                                     24, 25, 26, 27, 28, 29, 30,
    #                             31, 32, 33, 34, 35, 36],
    "random_originators": [1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 14, 15, 16, 17, 18, 19,
                           24, 25, 27, 28, 29, 30, 31, 32, 33, 35, 36, 100, 102,
                           104, 105, 106, 109, 110, 111, 121, 122, 123,
                           124, 125, 127, 128, 129, 130, 132, 133, 134, 135,
                           136, 137, 138, 139, 140, 141, 143, 144, 145, 148,
                           149, 151, 153],

    "num_originators": [10],
    #"num_originators": [60],
    "epochs_per_cycle": 100,            # number of different originator combinations

    "start_epoch": 20,                  # the first epoch where all U originators will start
                                        # disseminating their data

    "seed": 123,
    "payload": 2,                       # 0 = 23B frame

    # Testbed run paramenters
    "start_time": "asap",
    "duration": 240
}
