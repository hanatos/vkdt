# usm: sharpen via simple unsharp mask

this implements simple unsharp mask sharpening. it operates on a pixel level
and is resolution dependent. it is best applied just before the output, in
display referred after a curve.

## connectors

* `input`
* `output`

## parameters

* `amount` how much to increase sharpness
* `thrs` the threshold to not boost noise
