# raw input module

this module uses the rawspeed library to decode raw photographs.

## parameters

* `filename` the input file name
* `noise a` the gaussian part of the gaussian/poissonian noise model
* `noise b` the poissonian parameter of the same

if both noise parameters are set to `0.0`, `vkdt` will load the noise profiles
from `data/nprof/*`. see [noise profiling](../../../../doc/noiseprofiling.md)
for how to create a profile.
