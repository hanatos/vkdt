# ca: remove chromatic aberrations by defringing

this simple module detects edges and removes coloured fringes
which are caused by any kind of chromatic aberration (or other issues,
for that matter).

since it tries to detect edges and saturation (with the assumption that r=g=b
is "white"), it depends on a reasonably white balanced colour space, i.e. place
it after the `colour` module.

## parameters

* `thrs` an edge detection threshold, everything above this is an edge
* `amount` if you don't want to desaturate all the way, you can dial the effect down here

## connectors

* `input` the input buffer with chromatic aberrations, after the colour module
* `output` the output buffer without chromatic aberrations
