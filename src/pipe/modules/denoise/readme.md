# denoise

this reads a [noise profile](../../../../doc/noiseprofiling.md) and then runs
some variant of a-trous fisz-transform wavelets for noise reduction.
if you did not profile your camera for noise coefficients at the given iso value yet,
you can manually dial in the gaussian and poissonian part of the noise model
in the [raw input](../i-raw/readme.md) module.
you can run `denoise` pre- and post-demosaicing.

## connectors

* `input` the input image can be `rggb` or `rgba`
* `output` the output image will be the same format as the input


## params

* `strength` the overall strength of the wavelet thresholding. this directly scales the unbiased soft shrinkage threshold.
* `chroma` this affects the threshold only for the `yuv` colour channels
* `luma` this blends back a portion of the original `y` channel after denoising


## examples
