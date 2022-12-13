# i-raw: raw image input via rawspeed

this module uses the rawspeed library to decode raw photographs.

## parameters

* `filename` the input file name
* `noise a` the gaussian part of the gaussian/poissonian noise model
* `noise b` the poissonian parameter of the same
* `startid` the first image in a timelapse series

if both noise parameters are set to `0.0`, `vkdt` will load the noise profiles
from `data/nprof/*`. see [noise profiling](../../../../doc/howto/noise-profiling/readme.md)
for how to create a profile.

## timelapses

this module supports loading sequences of raw files. the raw files
need to be same orientation, same resolution, same camera, and
currently it's best to have same iso value too. all metadata will be
read once for the first image in the sequence and used for all the
other images too.

to use this feature, set `filename` to something like `IMG_%04d.CR2`
and `startid` to 944 if your first image is called `IMG_0944.CR2`. you
can then use the animation feature of `vkdt` to play back the
sequence. in the `.cfg`, be sure to set `frames:10` if you have ten
subsequently numbered raw files, and set `fps:1` for one frame per
second. if you set `fps` to something faster than your ssd/gpu can
deliver, you will experience frame drops.

you may want to checkout the keyframes feature to gradually modify
exposure for instance (see `examples/keyframes.cfg`, or the `k`
shortcut to create keyframes from the gui when hovering over controls.
