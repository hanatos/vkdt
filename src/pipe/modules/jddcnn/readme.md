# jddcnn: joined denoising and demosaicing convolutional neural network

this implements an integrated version of Benoit Brummer's cnn denoiser
based on his raw natural image dataset. in particular this is compatile
with [the weights produced by this fork here.](https://github.com/hanatos/rawnind_jddc/tree/jddcnn)
the fork replaces the u-net by a slightly simpler one, following the
open image denoise architecture by Attila Afra.

<div class="compare_box">
<textarea readonly style="background-image:url(denoise-off.jpg)"></textarea>
<textarea readonly style="background-image:url(denoise-on.jpg)" ></textarea>
</div>

the weights are read from `~/.config/vkdt/data/jddcnn-weights-heavy.dat` or,
for the slightly more light-weight variant with only 16 base channels
`~/.config/vkdt/data/jddcnn-weights-light.dat`.
download links will follow once the training converged.

in the pipeline it replaces denoising and demosaicing.
at the time of writing, the input should be the output of the `denoise` module, with
`strength` set to zero (i.e. crop black borders and rescale black and white points only).

the input is rggb bayer data in four planes of half resolution, scaled to the [0,1] range.
that is, the input should be *after* the `denoise` module (but be sure to leave the
denoising strength at 0). this way, the black borders will be cropped and the data
will be rescaled to the correct range.

the original glsl cooperative matrix convolution code was written by [Adrien
Vannson](https://github.com/AdrienVannson/gpu-denoising.git).

a word of warning: this module is *extremely slow*. it can take like a full
second on a 42MP image (RTX 3080 Ti). you might want to directly route out the result
via `o-exr` (maybe after input device transform in the `colour` module) and then
do the rest of the grading on the exr for faster interaction.

as a sidenote, [working with neural networks is pretty exciting](https://youtu.be/4h-wVe9a6rQ?t=116).

## parameters

* `model` use the heavy model (8.6M parameters) or the lighter one with 4M parameters

## connectors

* `input` a raw rggb bayer pattern image, after the denoise module scaled it to [0,1]
* `output` the denoised and demosaiced rgb image
