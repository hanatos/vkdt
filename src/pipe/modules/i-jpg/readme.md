# i-jpg: jpeg image input

this module reads jpeg from the input. to make it useful, you may want to
include a [`resize` module](../resize/readme.md) to correctly scale the image
for thumbnails. the generic [`colour` module](../colour/readme.md) will take
care that the data is brought into linear rec2020 colour space. for this, the
`i-jpg` module will tag the buffer according to the input. you can overwrite
this detection of colour primaries and tone response curve (trc).

possible choices for primaries are currently: `custom` indicating the use of
the camera matrix as loaded from a raw file for instance, `sRGB`, `rec2020`,
`AdobeRGB`, `P3`, `XYZ`.

possible choices for the trc are: `linear`, `709` (SMPTE RP 431-2),
`sRGB` (IEC 61966-2-1), `PQ` (SMPTE ST 2084), `DCI` (SMPTE ST 428-1),
`HLG` (Rec. ITU-R BT.2100-1), and `gamma`.

note that this module currently has (extremely limited) support to parse
an embedded icc profile from the jpg, and this info will be passed on
through the `prim` and `trc` parameters.


## parameters

* `filename` the filename to load. can include a "%04d" template for timelapses
* `startid` the start index. this + frame number will be filled into the "%04d" template
* `prim` the primaries used to encode this file
* `trc` the tone response curve applied to the data in this file (this will be undone to go to linear in the colour module)

## connectors

* `output`: the 8-bit rgba srgb data read from the jpeg image.
