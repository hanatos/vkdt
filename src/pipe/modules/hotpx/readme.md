# hotpx: remove stuck pixels from raw images

works on bayer and x-trans raw data. ideally run on the raw data
directly, i.e. before the denoise and hilite modules.

a photosite is replaced by the mean of its colour around it if it stands more
than `sigma` deviations of the noise, from the
[noise profile](../../../../doc/howto/noise-profiling/readme.md) or `noise`
without one, or of the local spread above that mean. around it means the 3x3
blocks of the mosaic around the photosite's own (3x3 photosites for x-trans,
2x2 for bayer), so that the statistics are shared by the photosites of a
block. the mean is estimated without the brightest two, so that hot photosites
next to each other still stand out, and corrected for leaving them out, so that
all colours are judged alike however few of them are around.

it is kept if at least two others in the 5x5 photosites around it are raised
too, one of them of another colour, each by `raised` deviations of the noise,
by one and a half of the local spread and by a quarter of the photosite's
excess: a real light reaches the other colours through the lens, a defect does
not. a glint confined to a pair of photosites, or to up to three of the same
colour, looks like a defect and is removed.

## connectors

* `input` the raw input directly after i-raw
* `output` a raw buffer with hot pixels filtered out

## parameters

* `noise` the deviation of the noise to assume without a noise profile,
  relative to the range from black to white. a profile separates hot pixels
  from noise much better, as it knows how the noise grows with the level
* `channels` which colours to filter
* `sigma` how many deviations (of the noise, or the local spread) above its
  colour around a photosite has to be to count as an outlier. lower removes
  weaker hot pixels, and more noise with them
* `raised` how many deviations of the noise the photosites around an outlier
  have to rise to count as a real light, which keeps it. lower keeps more
  stars and small lights, higher removes more hot pixels that sit in a
  noisy or busy neighbourhood
