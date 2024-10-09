# denoise: reduce image noise based on noise profiles

this reads a [noise profile](../../../../doc/howto/noise-profiling/readme.md) and then runs
some variant of a-trous fisz-transform wavelets for noise reduction.
if you did not profile your camera for noise coefficients at the given iso value yet,
you can manually dial in the gaussian and poissonian part of the noise model
in the [raw input](../i-raw/readme.md) module.
you can run `denoise` pre- and post-demosaicing.

the `denoise` module also takes care to crop off the black borders (noise and
black point calibration pixels) and removes the black point and scales to
the white point of the raw image file. it is thus an essential part of every
raw pipeline. set `strength` to `0.0` if you only want to crop/scale and no
denoising. it will employ a specialised `noop` kernel in this case.

this module is usually fast and denoises a full resolution 24
megapixel raw image in around 20ms on a lower end nvidia gtk
1650 max-q.

for extreme low-light cases if you have a chance to take burst photographs,
you can use the [align](../align/readme.md) module for further noise reduction.

## connectors

* `input` the input image can be `rggb` or `rgba`
* `output` the output image either mosaic (normalised with black point subtracted) or full rgb, depending on input


## parameters

* `strength` the overall strength of the wavelet thresholding. this directly scales the unbiased soft shrinkage threshold.
* `luma` this blends back a portion of the original `y` channel after denoising
* `detail` protect what is detected as detail. use lower values for extremely heavy noise where detail detection fails.
* `gainmap` if the input file ships metadata about a gainmap, it can be applied here


## examples

a bayer image after lens corrections, demosaicing after denoising (slide
the little right corner for comparison):  
<div class="compare_box">
<textarea readonly style="background-image:url(denoise-off.jpg)"></textarea>
<textarea readonly style="background-image:url(denoise-on.jpg)" ></textarea>
</div>

an xtrans image:  
<div class="compare_box">
<textarea readonly style="background-image:url(xtrans-off.jpg)"></textarea>
<textarea readonly style="background-image:url(xtrans-on.jpg)" ></textarea>
</div>

