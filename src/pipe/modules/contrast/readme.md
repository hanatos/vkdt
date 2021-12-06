# local contrast

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
