# Crystal Test

Crystal Test lets users evaluate Crystal. Crystal is based on the Glossy implementation atop Contiki for the DW1000 radio.


## Configuration and compilation

It is recommended to use the script `test_tools/simgen_ta.py` to compile batches of tests.
The sets of test parameters can be specified in `params.py`. It is recommended to have a `params.py`
in a separate directory dedicated for a specific set of tests. One can use the parameter
file in `exps/example_experiment` directory as an example.

Once you have a `params.py` defining the parameter sets, call the `simgen_ta.py` from the directory
of `params.py`. The script will automatically load the parameters from `params.py` in the current directory
and generate a separate subdirectory for every combination of parameters of the set.
