# Crystal on TSM
This folder contains an implementation of [Crystal](https://dl.acm.org/doi/10.1145/2994551.2994558) on top of [TSM](dev/dw1000/tsm).
This implementation has two variants from which you can choose. One uses the classic Crystal logic. The other adds [Flick]() to further improve the energy consumption of the protocol when using it with very sparse, aperiodic traffic patterns.

## How to use this layer
To use this implementation of Crystal on top of TSM you first need to enable [TSM](dev/dw1000/tsm). You can do this by defining `UWB_WITH_TSM = 1` in your application Makefile.
Your will also need to enable the Glossy layer by defining `UWB_WITH_TSM_GLOSSY = 1` in your application Makefile and then enable the Crystal TSM layer using `UWB_WITH_TSM_CRYSTAL = 1`

You can then build your application on top of Crystal by providing the same functions discussed in the [non-TSM crystal implementation](dev/dw1000/crystal).
Note that if you want to use the Flick-enabled version of this layer you will, in addition, need to provide the following functions (detailed in [crystal_tsm.h](dev/crystal_tsm/crystal_tsm.h)):
- `bool app_is_originator()`
- `bool app_has_packet()`

## Publications
This implementation was used in ["Network On or Off? Instant Global Binary Decisions over UWB with Flick"]()
```
@inproceedings{...}
```
