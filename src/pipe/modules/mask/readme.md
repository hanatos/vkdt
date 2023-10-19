# mask: create tunable parametric masks for use with blending

this module allows you to mask regions of the input image
based on luminance and colour ranges

## parameters

* `mode` select based on which property: luminance, hue, or saturation
* `vmin` the floor level of the mask
* `vmax` the maximum value of the mask
* `envelope` four values to define the begin and end of ease-in and ease-out of an envelope function

## connectors

* `input` the input colour image
* `output` the mask created for the image. plug this into a blend module for instance.
