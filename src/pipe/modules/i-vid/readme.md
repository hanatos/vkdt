# i-vid: video file input via ffmpeg

the `i-vid` module uses ffmpeg's backend libraries (avcodec/avformat) to read
compressed video streams as input.

## parameters

* `colour` colour space of the input video
* `trc` tone response curve in the encoding
* `bitdepth` bit depth of the input video stream (actually immutable but displayed here for your information)
* `chroma` subsampling of the chroma planes (actually immutable but displayed here for your information)
* `colrange` the colour range (full or restricted)
