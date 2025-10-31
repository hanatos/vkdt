# jddcnn: joined denoising and demosaicing convolutional neural network

this implements an integrated version of Benoit Brummer's cnn denoiser
based on his raw natural image dataset. in particular this is compatile
with [the weights produced by this fork here.](https://github.com/hanatos/rawnind_jddc/tree/jddcnn)

the weights are read from `~/.config/vkdt/data/jddcnn-weights.dat`.

in the pipeline it replaces denoising and demosaicing.
at the time of writing, the input should be the output of the `denoise` module, with
`strength` set to zero (i.e. crop black borders and rescale black and white points only).

the network architecture is a u-net very much inspired by intel's OIDN.
the input is rggb bayer data in four planes of half resolution.

the original glsl cooperative matrix convolution code was written by [Adrien
Vannson](https://github.com/AdrienVannson/gpu-denoising.git).

as a sidenote, [working with neural networks is pretty exciting](https://youtu.be/4h-wVe9a6rQ?t=116).

## parameters

## connectors

* `input` a raw rggb bayer pattern image, after the denoise module scaled it to [0,1)
* `output` the denoised and demosaiced rgb image
