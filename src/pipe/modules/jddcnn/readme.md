# jddcnn: joined denoising and demosaicing convolutional neural network

this implements an integrated cnn denoiser based on Attila Afra's open image
denoise architecture. in particular this is compatile with [the weights
produced by this fork here](https://codeberg.org/hanatos/oidn/src/branch/main/training).
the fork introduces minor differences to the u-net, mainly leaky relu
parameter, channel counts, and pixel shuffles on both ends to account for bayer and xtrans
patterns. for full colour denoising, please use the [`oidn`](../oidn/readme.md) module instead.

<div class="compare_box">
<textarea readonly style="background-image:url(denoise-off.jpg)"></textarea>
<textarea readonly style="background-image:url(denoise-on.jpg)" ></textarea>
</div>
image by piratenpanda (pixls.us), CC-BY-SA

the weights are read from `~/.config/vkdt/data/jddcnn-bayer.dat` or
from `~/.config/vkdt/data/jddcnn-xtrans.dat`.
download links:
* [bayer](https://github.com/hanatos/vkdt/releases/download/1.1.0/jddcnn-bayer.dat)
* [xtrans](https://github.com/hanatos/vkdt/releases/download/1.1.0/jddcnn-xtrans.dat)

in the pipeline it replaces denoising and demosaicing. the input is scene
referred mosaic data. that is, the input should be *after* the `denoise` module
(but be sure to leave the denoising strength at 0). this way, the black borders
will be cropped and the data will be rescaled to the correct range.

as a sidenote, [working with neural networks is pretty exciting](https://youtu.be/4h-wVe9a6rQ?t=116).

## parameters

* `arch` use a network compiled for the given gpu architecture. best leave this on auto. others may or may not work on your device.
* `variant` pick network weights optimised for clean or noisy images. trades noise reduction and sharpness.

## connectors

* `input` a raw mosaic pattern image (bayer or xtrans), after the denoise module scaled it to [0,1]
* `output` the denoised and demosaiced rgb image

## ethics

the demosaicing/denoising models here have been trained on a couple hundred of
random holiday pictures that i happened to find on my harddrive. for the
limited receptive fields and the application here it doesn't seem to matter so
much what's on the images (apart from edges, gradients, patterns). the
synthetic training data generator will fuck them up beyond any recognition: cut
into small tiles, flip, switch colour channels, etc. the data is thus by no
means illegally scraped off dubious sources from the internets by big tech, as
you would otherwise expect. the training itself also finishes on a single
desktop machine in an hour or two, so if you want to be absolutely sure it's
easy enough to retrain for yourself on your own data.
