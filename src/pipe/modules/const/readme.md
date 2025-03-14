# const: generate constant image

generate a buffer with constant solid colour. it maintains some
compatibility with colour picker parameters as to be a drop-in
replacement for this module for static cases.

## connectors

* `output` the rendered image

## parameters

* `nspots` number of colour spots (<=24)
* `colour` one colour per spot
* `mode` whether this module is used as a regular image buffer or as a colour picker replacement
* `wd` the width of the output image buffer
* `ht` the height of the output image buffer
