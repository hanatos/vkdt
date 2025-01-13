# vis: visualise input data with colour ramps

this converts the luminance of the linear input signal to an srgb viridis colour map,
like in the following example:

[![viridis map](scatter.jpg)](scatter.jpg)

note that the output will be sRGB, so if you need rec2020, insert an instance
of `colour` after this and set it to `image` mode.


## connectors

* `input` rec2020 values, the luminance will be visualised
* `output` the sRGB viridis coloured version of the input

## parameters

* `exposure` the exposure correction value in stops
