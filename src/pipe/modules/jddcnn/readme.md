# jddcnn: joined denoising and demosaicing convolutional neural network

disclaimer: this module is in a highly experimental stage and is probably
not very useful for productive use.

the weights are read from `~/.config/vkdt/data/jddcnn-weights.dat`.

this executes a network trained externally (pytorch ipython notebook).
in the pipeline it should replace denoising and demosaicing.

the network architecture is a u-net very much inspired by intel's OIDN.
the input is rggb bayer data in four planes of half resolution, and a fifth input channel
is added as a noise estimate for the bayer block, using the poissonian/gaussian noise
profile data.

the original code was written by [Adrien Vannson](https://github.com/AdrienVannson/gpu-denoising.git)
and has been modified to work on bayer images and ported from tensorflow to pytorch.

as a sidenote, [working with neural networks is pretty exciting](https://youtu.be/4h-wVe9a6rQ?t=116).

## TODO

compatibility:
* make it work for non-coopmat devices (compile two versions of the shaders)

for optimisation:
* main loop over channels: use workgroup dimension z to parallelise more for more channels?
* uvec4 loads (though i think i don't want to be doing this)
* look at nvidia coopmat gemm sample and at tencent ncnn 3x3 convolution kernel for comparison

for quality:
* oidn has some extra convolutions on input/output, does it help quality?
* try loss after tone curve

## parameters

* `black` custom raw black level
* `white` custom raw white level
* `noise_a` gaussian part of the noise model
* `noise_b` poissonian part of the noise model

## connectors

* `input` a raw rggb bayer pattern image, after the denoise module scaled it to [0,1)
* `output` the denoised and demosaiced rgb image
