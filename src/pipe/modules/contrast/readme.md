# contrast: local contrast based on guided filter

increase or decrease local contrast in an image.
this utilises [the guided filter](../guided/readme.md) to extract a coarse image.

## connectors

* `input`
* `output`

## parameters

* `radius` the radius in effect for the guided filter
* `edges` the edge threshold parameter for the guided filter
* `detail` the amount of detail enhancement (as for unsharp masking)
* `thrs` the threshold used for detail enhancement (as for unsharp masking)

## use cases

use this module at the default settings to enhance texture in the image, while
leaving edges as they are.

to emphasise edges instead, while leaving texture as it is, you would
increase the `edges` parameter to say 0.5 and also up the `thrs` parameter
to say 0.003 to make the algorithm ignore texture detail.

use sparingly!
