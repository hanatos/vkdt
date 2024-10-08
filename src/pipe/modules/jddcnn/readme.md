# jddcnn: joined denoising and demosaicing convolutional neural network

this module should be run on rggb bayer input data.

TODO: not normalised and no gainmap applied, we can do this too (as denoise did)?

this executes a network trained externally (pytorch ipython notebook).
in the pipeline it should replace denoising and demosaicing, though it expects
the data normalised to [0, 1] range (can contain negative values in case of noise).

the network architecture is a u-net very much inspired by intel's OIDN.
the input is rggb bayer data in four planes of half resolution, and a fifth input channel
is added as a noise estimate for the bayer block, using the poissonian/gaussian noise
profile data.

the original code was written by [Adrien Vannson](https://github.com/AdrienVannson/gpu-denoising.git)
and has been modified to work on bayer images and ported from tensorflow to pytorch.

# TODO
* finish wiring in main.c
* b/w scaling, upload right values for input params

compat:
* replace NV by coopmat wrapper

for optimisation:
* put rescaling/gainmap in ingest?
* sort max pooling operations to *output* to reduce global memory traffic
* main loop over channels: use workgroup dimension z to parallelise more for more channels?
* uvec4 loads (though i think i don't want to be doing this)
* look at nvidia coopmat gemm sample and at tencent ncnn 3x3 convolution kernel for comparison

