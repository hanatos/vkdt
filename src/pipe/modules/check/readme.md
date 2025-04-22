# check: gamut and exposure checking

this module marks out of gamut and over- or underexposed pixels in the image.

## parameters

* `thrs` tolerance threshold, increase to mark more pixels
* `mode` mark out of gamut and/or exposure
* `gamut` if in gamut mode, mark colours outside the selected gamut
* `channels` switch individual channels in the image off selectively
