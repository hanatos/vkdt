# blend: combine two images

this one takes the inputs and combines them to the output image. the
mode of operation is controlled by the parameters mode and opacity.


## parameters

* `mode` defines the blend mode
* `opacity` some blend modes use an opacity value
* `taathrs` the threshold for temporal anti aliasing

## connectors

* `input` the new top layer
* `back` the back buffer of the composite
* `mask` an optional mask used for certain blend modes. connect a dummy if you don't need it
