# o-jpg: write jpg files

supports writing jpg images to disk. [convert to srgb or a similar colour space](../colenc/readme.md)
before connecting to this module.

## connectors

* `input` the 8-bit srgb buffer to be written to disk

## parameters

* `filename` the filename on disk to write to. `.jpg` will be appended.
* `quality` 0-100 jpeg quality
