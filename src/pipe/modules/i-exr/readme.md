# i-exr: read openexr image files

this reads `.exr` files if they are 3 channel half float encoded.

## parameters

* `filename` the name of the input file, potentially containing %04d for sequences
* `startid` the index of the first file if loading a sequence
* `noise a` the gaussian portion of the noise model
* `noise b` the poissonian portion of the noise model
* `prim` the primaries used to encode this file
* `trc` the tone response curve applied to the data in this file (this will be undone to go to linear in the colour module)

## connectors

* `output` half floating point precision pixel data
