# blend: combine two images

this one takes the inputs and combines them to the output image. the
mode of operation is controlled by the parameters mode and opacity.

current blend modes:

* `over` the output will be (1-opacity) * back + opacity * input
* `taa` use temporal anti-aliasing with box clamping and opacity as parameter
* `focus stack` wavelet transform both back and input and keep only the detail coefficients with larger magnitude
* `multiply` multiplies the back buffer and input


## parameters

* `mode` defines the blend mode
* `mask` use or ignore the mask connector
* `opacity` some blend modes use an opacity value
* `taathrs` the threshold for temporal anti aliasing

## connectors

* `input` the new top layer
* `back` the back buffer of the composite
* `mask` an optional mask used for certain blend modes. connect a dummy if you don't need it
