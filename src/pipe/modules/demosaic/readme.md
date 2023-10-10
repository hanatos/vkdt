# demosaic: interpolate cfa in raw images

this module applies some gaussian splatting to mosaiced raw
sensor images. it works transparently for bayer or xtrans sensors.

does not handle black or white points nor crop black borders.
this is done in [the denoise module](../denoise/readme.md).

## connectors

* `input` mosaic input with single channel per pixel
* `output` demosaiced output with colour per pixel

## parameters

* `colour` select sharp for collecting colour only in a small window.
   the smooth option is using a larger window and slower to process. use
   only when you encounter artifacts.
* `method` choose between the demosaicing methods. some only work for bayer
   patters (not fuji xtrans).
