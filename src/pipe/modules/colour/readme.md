# colour processing

this is a unified module to handle important things concerning colour.

it works on camera rgb data, applies camera white balance coefficients and if
desired a 3x3 matrix. this kind of input transform is known to be imprecise
especially for saturated colours and produce all kinds of artifacts (say way
out of gamut dark blue tones). to alleviate this, this module comes with a
corrective radial basis function (RBF) applied after white balance and matrix.
it allows to map arbitrary source to target points in 2d chromaticity space.

this mechanism is used for gamut mapping: a few points are sampled on the
input rgb cube, converted to output, and then projected onto a desired
gamut. this can be rec709 or the full spectral locus (approximated by a
10-vertex polygon).

it can also be used to calibrate the input colour against a measured
test chart (for instance an IT8), or for artistic colour manipulation.

the chromaticity RBF operates in rec2020, normalised to r/(r+g+b) and b/(r+g+b).
