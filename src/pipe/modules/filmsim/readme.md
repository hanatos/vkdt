# filmsim: artic's sophisticated spectral analog film simulation

TODO
* put documentation/nice example images
* DIR coupling
* create main.c, handle enlarged output buffer

## connectors

* `input` scene referred linear rec2020 (after the colour module)
* `output` the exposed, developed, and printed film simulation
* `filmsim` the filmsim.lut with the film data
* `spectra` wire data/spectra-em.lut, the spectral upsampling table for emission

## parameters

* `film` the film id in the datafile
* `ev film` exposure correction when exposing the film
* `g film` gamma correction for exposing the film
* `paper` the paper id in the datafile
* `ev paper` exposure correction when sensitising the paper
* `g paper` gamma correction when sensitising the paper
* `grain` switch grain simulation on or off
* `size` scale the grain size
* `uniform` uniformity of the grains as seen through the pixels. 1.0 means no variation at all
* `enlarger` mode of exposing the paper: output straight film negative or resize output
* `filter m` when exposing the print paper, dial in this share of magenta filter
* `filter y` when exposing the print paper, dial in this share of yellow filter
