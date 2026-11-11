# o-jxl: write JPEG-XL files

supports writing still RGB jxl images to disk. currently only supports sRGB, rec2020, AdobeRGB and P3 primaries.

## connectors

* `input` the half-float buffer to be compressed and written to disk. re-quantisation of values to lower bit-depths can be done within the encoder.

## parameters

* `filename` the filename on disk to write to. `.jxl` will be appended.
* `quality` 0-100 where 100 sets lossless encoding. other values intended to roughly match libjpeg-turbo quality.
* `effort` 1-10 (default = 7) where higher values allow more computation. for lossless this generally means smaller file sizes. for lossy this means better visual quality at a given file size, but also greater conformance to the specified quality. importantly this can sometimes mean larger (but higher quality) images than at lower effort levels.
* `bits` for lossless this will convert the f16 input to the specified bit depth (if not kept at original). for lossy this just signals to the decoder what bit depth to output.
* `custom i` whether to set a custom intensity target. default intensity targets are chosen by the encoder.
* `target i` custom intensity target in nits. affects how much lossy compression ensures smooth gradation and is maybe, sometimes, treated as a maximum content luminance value?
* `exif` whether to write exif data copied from the original input image
