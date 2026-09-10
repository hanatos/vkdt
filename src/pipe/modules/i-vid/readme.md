# i-vid: video file input via ffmpeg

the `i-vid` module uses ffmpeg's backend libraries (avcodec/avformat) to read
compressed video streams as input.

## parameters

* `colour` colour space of the input video
* `trc` tone response curve in the encoding
* `bitdepth` readonly: bit depth of the input video stream (actually immutable but displayed here for your information)
* `chroma` readonly: subsampling of the chroma planes (actually immutable but displayed here for your information)
* `colrange` readonly: the colour range (full or restricted)

## TODO

* create sampler should respect full/mpeg range and YCBCR 709 vs 2020
* bg thread for video decoder
