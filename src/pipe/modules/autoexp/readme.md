# autoexp: automatic exposure via moving average

this module is useful for animations with highly varying
dynamic range over time.

## connectors

* `input`
* `output`

## parameters

* `vup` the speed of exposure adjustment going brighter
* `vdown` the speed of exposure adjustments towards darker
* `timediff` the anticipated time difference between frames, affects the speed of adjustment
* `reset` set to 1 to restart metering from zero instead of running average over time
