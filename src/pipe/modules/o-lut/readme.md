# o-lut: write vkdt lookup table files

this is yet another file format which supports uncompressed data with varying
number of channels (graph output is limited to 4 per pixel) and types (`f16`
and `f32`).

## connectors

* `input` : the data to be written do disk

## parameters

* `filename` : the filename to be written to disk. `.lut` will be appended.
