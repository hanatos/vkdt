# hdrmerge: combine images of different exposures to recover high dynamic range radiance maps 

combine multiple images of the same composition into a single hdr image. this 
module is best applied before demosaic but after denoise.

this reads the shutter speed from the image to perform the calculation. 
to perform the hdrmerge, take multiple still photos at same position with 
varying shutter speeds. keep the aperture and iso constant across shots.

for radiance map output, it may be beneficial to put a reinhard tonemap before the 
filmcurv and adjust the light param on the filmcurv module to the desired value

## parameters

* `pltscale` axis scale of dspy plot
* `output` either exposure or radiance
* `ev_push` for exposure output mode

## connectors

* `input` the primary input. note, the camera properties will be passed through this input
* `input2` the secondary input to merge with
* `input3` (optional) the third input to merge with
* `input4` (optional) the fourth input to merge with
* `output` output of the merged images. either exposure or radiance
* `dspy` a log-log plot of log irradiance (lnE) vs the pixel output (Z)

## technical
