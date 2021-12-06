# vignetting / devignetting

## connectors

* `input`
* `output`

## parameters

* `coeff` : these are 14 32-bit floating point coefficients that define the vignette.

the coefficients encode in this order: 2d center, 2x2 transformation matrix, 4d polynomial coefficients for x, 4d polynomial coefficients for y aligned falloffs.
