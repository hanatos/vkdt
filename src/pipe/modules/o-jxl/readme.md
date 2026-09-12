# o-jxl: write JPEG-XL files

supports writing still RGB jxl images to disk. currently only supports sRGB, rec2020, AdobeRGB and P3 primaries.

## connectors

* `input` the half-float buffer to be compressed and written to disk

## parameters

* `filename` the filename on disk to write to. `.jxl` will be appended.
* `quality` 0-100 where 100 sets lossless encoding (of the f16 input values). other values intended to roughly match libjpeg-turbo quality.
* `effort` 1-10. higher values allow more computation. for lossless this generally means smaller file sizes. for lossy this means better visual quality at a given filesize, but also greater conformance to the specified quality. importantly this can sometimes mean larger (but higher quality) images than at lower effort levels.
* `exif` whether to write exif data copied from the original imput image
