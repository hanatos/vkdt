# spatiotemporal variance guided filtering

denoise real time rendering input corrupted by monte carlo noise.

this module follows the paper
Schied, Kaplanyan, Chaitanya, Burgess, Dachsbacher, and Lefohn:
*Spatiotemporal Variance-Guided Filtering: Real-Time
Reconstruction for Path-Traced Global Illumination*, HPG 2017.

the pipeline is as follows. on the input is a buffer with albedo-demodulated
irradiance and the albedo comes in separately. the noisy irradiance goes
through a pre-blend pass, which merges previous samples into one buffer. this
is controlled by a blendweight parameter `lalpha` and samples are rejected
based on depth and normal difference. next, the still-noisy buffer is passed
through 4 iterations of edge avoiding a trous wavelets. the edges are
determined as in the original paper, but we compute variance slightly
differently (filter the moments, not the standard deviation).
the denoised output is then undergoing albedo modulation (denoised irradiance
and albedo buffers are multiplied) and combined with the same from the last
frame via taa with box clamping.

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
