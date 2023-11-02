# lenswarp: correct for lens distortion and TCA

This warps the image in order to correct for lens distortion and / or transverse chromatic aberration.

This must be placed after demosaic and before crop for correct operation.

The correction parameters are automatically obtained from:

* for [DNG](https://helpx.adobe.com/camera-raw/digital-negative.html) files: the `WarpRectilinear` opcode in the `OpcodeList3` tag

If there are no supported correction parameters embedded in the file, this module does nothing.

In some cases the correction may result in black areas visible around the edges of the image, or more of the image being cropped away than is strictly necessary to avoid black areas. This can be corrected by adjusting the `scale` parameter. Regardless of the `scale` setting, the output is always the exact same size as the input.

## connectors

* `input` the input image
* `output` the output image after applying correction

## parameters

* `scale` scale the image larger or smaller
