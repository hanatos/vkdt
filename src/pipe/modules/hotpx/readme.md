# hotpx: remove stuck pixels from raw images

currently only work on bayer pattern images. ideally run on the raw data
directly, i.e. before the denoise and hilite modules.

## connectors

* `input`
* `output`

## parameters

* `thrs` the threshold to classify an outlier pixel
