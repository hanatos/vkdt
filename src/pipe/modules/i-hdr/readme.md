# i-hdr: greg ward's rgb+exponend high dynamic range format

this reads `.hdr` files, i.e. rgbe high dynamic range floating
point maps.

## parameters

* `filename` the name of the input file, potentially containing %04d for sequences
* `startid` the index of the first file if loading a sequence

## connectors

* `output` half floating point precision version of the rgbe input file.
