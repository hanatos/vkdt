# nprof: estimate parameters for noise profiling

this output module fits a gaussian/poissonian noise model and creates noise
profile files to be stored in `~/.config/vkdt/nprof` or `nprof/` and read back in by `vkdt`.

## parameters

* `test` set the noise profile parameters in the file written out in the `i-raw:main` module for inspection
* `install` install the noise profile to the home directory
* `remove` remove the raw histogram and noise profile modules from the graph

## connectors

* `input` the single-channel `ui32` noise profile data from [the raw histogram](../rawhist/readme.md)
