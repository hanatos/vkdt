# o-pfm: write netpbm float map files

output an rgb, little endian, non-flipped pfm image. will not otherwise
change the pixels (i.e. you input linear rec2020, it'll write that to disk).

## connectors

* `input` : the `rgba f16` data to be written do disk

## parameters

* `filename` : the filename to be written to disk. `.pfm` will be appended.
