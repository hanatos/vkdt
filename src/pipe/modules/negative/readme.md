# negative: invert scanned film negatives

this corrects scanned negative films to produce a positive image as output.
since the process depends on the film stock as well as the device used to
digitise it, you have to decide where to put the image matrix (in the `colour`
module). for a photograph of film, put the `colour` module first, correcting
the camera's response (keeping exposure and the rest at identity values). you
may then want to use a second instance of the [`colour module`](../colour/readme.md),
with matrix set to `rec2020`, placed after the `negative` module in the graph,
to change exposure and for other artistic changes to colours. for further
refinement of the image, you may want to use the [`grade`](../grade/readme.md)
module after `negative` and `colour`.

## parameters

* `Dmin` the minimum density of the film

## connectors

* `input` the photographed/scanned negative, linear after the colour module
* `output` the positive image
* `dmin` optional input, connect a const module (converted from a colour picker picking the min film density) 
