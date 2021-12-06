# noise profiling

this output module fits a gaussian/poissonian noise model and creates noise
profile files to be stored in `data/nprof/` and read back in by `vkdt`.

## connectors

* `input` the single-channel `ui32` noise profile data from [the raw histogram](../rawhist/readme.md)
