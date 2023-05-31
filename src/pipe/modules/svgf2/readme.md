# svgf2: decimated wavelet denoising

TODO

# parameters

* `alpha` the taa blend weight for the previous frame. more means more blur
* `taa` the taa rejection threshold for the previous frame, in units of standard deviation
* `prealpha` controls the exponentially weighted average of the pre-blend pass on the noisy
  input samples

# connectors

* `mv` the motion vectors to align the previous frame with the current
* `input` the noisy input, assumed to be irradiance
* `albedo` the input albedo, to be multiplied to the irradiance for final beauty
* `output` the final beauty output
* `gbufp` the previous gbuffer (normal, depth, 1st moment, 2nd moment)
* `gbufc` the current gbuffer (normal, depth, 1st moment, 2nd moment)
