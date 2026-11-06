# hotpx: remove stuck pixels from raw images

works on bayer and x-trans raw data. ideally run on the raw data
directly, i.e. before the denoise and hilite modules.

a photosite is replaced by the mean of its colour around it (5x5) if it
stands more than `sigma` deviations of the
[noise profile](../../../../doc/howto/noise-profiling/readme.md) or of the
local spread above that mean. the mean is estimated without the brightest two,
so that hot photosites next to each other still stand out, and corrected for
leaving them out, so that all colours are judged alike however few of them
are in the window.

it is kept if at least two others in the window are raised too, one of them of
another colour, each by a quarter of the photosite's excess: a real light
reaches the other colours through the lens, a defect does not. a glint
confined to a pair of photosites, or to up to three of the same colour, looks
like a defect and is removed.

## connectors

* `input` the raw input directly after i-raw
* `output` a raw buffer with hot pixels filtered out

## parameters

* `thrs` the threshold to classify an outlier pixel, relative to the range
  from black to white. used without a noise profile, which separates hot
  pixels from noise much better
* `channels` which colours to filter
* `sigma` how many deviations (of the noise, or the local spread) above its
  colour around a photosite has to be to count as an outlier. lower removes
  weaker hot pixels, and more noise with them
