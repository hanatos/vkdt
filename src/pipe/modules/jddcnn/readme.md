# jddcnn: joined denoising and demosaicing convolutional neural network

this module should be run on rggb bayer input data.

this executes a network trained externally (pytorch ipython notebook).
in the pipeline it should replace denoising and demosaicing.

the network architecture is a u-net very much inspired by intel's OIDN.
the input is rggb bayer data in four planes of half resolution, and a fifth input channel
is added as a noise estimate for the bayer block, using the poissonian/gaussian noise
profile data.

the original code was written by [Adrien Vannson](https://github.com/AdrienVannson/gpu-denoising.git)
and has been modified to work on bayer images and ported from tensorflow to pytorch.

## TODO

apply gainmap *before* denoising, for telephones it can be so extreme/throw off the rggb pattern

compat:
* make it work for non-coopmat devices (compile two versions of the shaders)

for optimisation:
* sort max pooling operations to *output* to reduce global memory traffic
* main loop over channels: use workgroup dimension z to parallelise more for more channels?
* uvec4 loads (though i think i don't want to be doing this)
* look at nvidia coopmat gemm sample and at tencent ncnn 3x3 convolution kernel for comparison

## parameters

* `black` custom raw black level
* `white` custom raw white level
* `noise_a` gaussian part of the noise model
* `noise_b` poissonian part of the noise model

## connectors

* `input` a raw rggb bayer pattern image
* `output` the denoised and demosaiced rgb image
