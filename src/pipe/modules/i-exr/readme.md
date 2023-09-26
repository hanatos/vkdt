# i-exr: read openexr image files

this reads `.exr` files if they are 3 channel half float encoded.

## parameters

* `filename` the name of the input file, potentially containing %04d for sequences
* `startid` the index of the first file if loading a sequence

## connectors

* `output` half floating point precision pixel data
