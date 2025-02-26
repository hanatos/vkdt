# filmsim: artic's sophisticated spectral film simulation

TODO
* put documentation/nice example images
* DIR coupling
* clean up parameters
* create main.c and do the lut wiring in the background, automatically
  XXX can't do this because node and not module callbacks!
  TODO unify all the luts into one
  TODO analytical fit for thorlabs filters


## connectors

* `input` scene referred linear rec2020 (after the colour module)
* `output` the exposed, developed, and printed film simulation

## parameters

