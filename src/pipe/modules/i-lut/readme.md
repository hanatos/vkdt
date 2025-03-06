# i-lut: lookup table input

some custom uncompressed input format that supports half float, 32-bit float,
1-, 2-, and 4-channel data.

this module supports loading arrays of textures, as for instance required by
the [`rt` module](../rt/readme.md). to do this, pass a filename that points to
a `.txt` file with one filename per line. the module will then load the lut
files referenced in the `.txt` file and pass them as an array connector.

## parameters

* `filename` the input filename substitution will consider "maker" "model" and "flen". pass a .txt file here to load a list of textures (one filename per line in the txt file)

## connectors

* `output` the output data with the corresponding channels
