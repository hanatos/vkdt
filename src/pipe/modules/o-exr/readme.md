# o-exr: write openexr image files

this writes `.exr` files, 3 channel half float encoded.
as of today, we don't write the chromaticities attribute (or any metadata), but
if you connect a regular working space module to this output, it will contain
linear bt2020 colour data.

## parameters

* `filename` the name of the output file

## connectors

* `input` half precision floating point rgba pixel data
