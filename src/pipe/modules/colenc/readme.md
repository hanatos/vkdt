# colenc: colour encode from working space (rec2020) to output

this module changes the colour space and shaper curve of the pixel data. it can
be used to convert the internal linear rec2020 data to a few combinations of
primaries and tone response curve (trc). the data will be tagged accordingly
and travel along the graph. some output modules pick up this information and
will write this information into the output file. for instance if you set
primaries to AdobeRGB and trc to gamma, the `o-jpg` module will embedd an
AdobeRGB icc profile in the jpeg file.

possible choices for primaries are currently: `custom` indicating the use of
the camera matrix as loaded from a raw file for instance, `sRGB`, `rec2020`,
`AdobeRGB`, `P3`, `XYZ`.

possible choices for the trc are: `linear`, `709` (SMPTE RP 431-2),
`sRGB` (IEC 61966-2-1), `PQ` (SMPTE ST 2084), `DCI` (SMPTE ST 428-1),
`HLG` (Rec. ITU-R BT.2100-1), and `gamma`.

## connectors

* `input` scene referred linear rec2020
* `output` encoded colour as specified by the parameters

## parameters

* `prim` identifies the primaries of the output colour space
* `trc` identifies the tone response curve
