# i-pfm: netpbm float map input

this reads `.pfm` files, i.e. simplistic netpbm little endian rgb floating
point maps. does not flip y coordinate.

## parameters

* `filename` the name of the input file, potentially containing %04d for sequences
* `startid` the index of the first file if loading a sequence

## connectors

* `output` half floating point precision version of the f32 input file.
