# Flick Test
This folder contains the application code used to test Flick, a UWB primitive for disseminating binary conditions.
We presented our findings in our paper, [Network On or Off? Instant Global Binary Decisions over UWB with Flick](), at IPSN2023.

This code can be used as a minimal example on how to use Flick. More complex examples can be found in our custom Flick-enabled implementations of [Crystal](https://dl.acm.org/doi/10.1145/2994551.2994558) and [Weaver](http://disi.unitn.it/~picco/papers/sensys20_weaver.pdf). In these implementations, Flick is used to disseminate the binary decision to stay awake (and collect more data) or let the radio enter sleep (and save energy).

More information about Flick and the Flick-enabled Crystal and Weaver implementations can be found in their respective implementations:
- Flick, which can be found inside the code of [TSM](dev/dw1000/tsm)
- [Flick-enabled Crystal](dev/dw1000/crystal_flick)
- [Flick-enabled Weaver](systems/weaver)

## Publications
This application was used to test Flick in [Network On or Off? Instant Global Binary Decisions over UWB with Flick]()
```
@inproceedings{...}
```
