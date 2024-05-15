# o-exr: write openexr image files

this writes `.exr` files, 3 channel half float encoded. the output exr file
will contain the chromaticities attribute. normally the pipeline will output
linear bt2020 colour data, but you can select different primaries during
export. the chromaticities attribute will contain coordinates for sRGB, bt2020,
AdobeRGB, P3 (D65), or XYZ.

the module will also write non-linear data to the output files, though there is
no standard attribute to be set in this case. there will be a warning on the
console if you do so, make sure you know what you're doing.

## parameters

* `filename` the name of the output file

## connectors

* `input` half precision floating point rgba pixel data
