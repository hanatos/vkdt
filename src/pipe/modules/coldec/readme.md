# coldec: convert input colour to linear working space (rec2020)

this module linearises input from some colour spaces. for instance most
jpeg files will come as sRGB, including a non linear tone response curve.
this is the inverse of the [`colenc` module](../colenc/readme.md).

possible choices for primaries are currently: `custom` indicating the use of
the camera matrix as loaded from a raw file for instance, `sRGB`, `rec2020`,
`AdobeRGB`, `P3`, `XYZ`.

possible choices for the trc are: `linear`, `709` (SMPTE RP 431-2),
`sRGB` (IEC 61966-2-1), `PQ` (SMPTE ST 2084), `DCI` (SMPTE ST 428-1),
`HLG` (Rec. ITU-R BT.2100-1), and `gamma`.

## connectors

* `input` encoded colour as specified by the parameters
* `output` scene referred linear rec2020

## parameters

* `prim` identifies the primaries of the output colour space
* `trc` identifies the tone response curve
