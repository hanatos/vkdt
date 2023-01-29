# negative: invert scanned film negatives

this corrects scanned negative films to produce
a positive image as output. since the process depends
on the film stock as well as the device used to digitise it,
the image matrix is usually not useful and it is a good idea
to set the colour matrix to rec2020 in the
[`colour module`](../colour/readme.md) which you should place
after the `negative` module in the graph. for further refinement
of the image, you may want to use the [`grade`](../grade/readme.md)
module after `negative` and `colour`.

## parameters

* `Dmin` the minimum density of the film

## connectors

* `input` the photographed/scanned negative
* `output` the positive image
