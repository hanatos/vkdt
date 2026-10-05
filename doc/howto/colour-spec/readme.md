# spectral colorimetry, matrix-free

in this article we show how to render an image from camera rgb without using matrices.

this is highly related to [input device colour management](../colour-input/readme.md), and
the `vkdt mkssf` tool which creates plausible spectral sensitivity functions for a camera
from either matrices in a dcp profile or colour checker calibration shots.


## image formation in a camera device

<img src="overview.svg" style="width:100%"/>

rendering an image from a photographic measurement works in several steps.
the source stimulus in the scene (a) is imaged through a device (b) with certain
spectral sensitivity in the colour filter array (CFA). this results in
tristimulus values, three numbers representing the stimulus in camera rgb.

this will in general be somewhat different to imaging the same stimulus
through a human observer (d). in particular, due to the reduction in
dimensionality, a set of spectra will collapse to the same tristimulus values
both in camera rgb (c) and in observer space (e). unfortunately these
aren't the same sets, they both have different metameric failures.

input device transforms (such as the one vkdt's `vkdt mkclut` generates)
will map camera rgb to XYZ, but unavoidably collect metameric failure
on the way.

upsampling (g) the tristimulus values reconstructs a credible spectrum from (c)
or (e). a side constraint is that the upsampled spectrum is metameric with the
real stimulus (a), under the camera CFA (b) or the CIE observer (d).
this spectrum can be used for more accurate spectral processing.


## spektrafilm

such spectral processing is for instance the *spektrafilm* film simulation,
implemented in the `filmsim` module. the spectral upsampling is performed
within the module, and it requires a look up table to do this computation. the
lut is supplied as input connector `spectra` and contains a mapping `xy` → `c0
c1 c2 c3`, where `c*` are coefficients for a function space representing smooth
emissive spectra (sigmoidal + scale).

the process is normally done by daisy-chaining input device transform (IDT in
the `colour` module) and then upsampling from the observer space. this is nice
and general, but inherits the problems of the IDT:

* out of gamut values for matrix transforms
* metameric failure is of the camera CFA *and* of the transform to observer coordinates XYZ.

taking a picture will always take the path `(a)→(b)→(c)`. there are multiple
paths from camera rgb (c) to upsampled spectrum (g). clearly the path
`→(f)→(e)→(g)→` collects more error than `→(g)→`. this is regardless of how
accurate we design (f), due to the double metameric failure involved here.
also, (g) will work on the correct assumptions when trying to come up with a
plausible spectrum similar to (a).

let's first look at our options for the IDT (f).



## input device transform recap

mapping camera rgb to observer space is a necessary step so subsequent tools as
well as the display colour management can work on some known and
camera-indepenent tristimulus space. most often, this is done by using a simple
3x3 matrix transform from camera rgb to XYZ. this matrix can for instance be
found by fitting the matrix coefficients to a picture of a colour checker
target with known XYZ reflectances, taken under known illuminant. this is a
smooth mapping with no additional metameric failure introduced (all camera rgb
values will receive their individual XYZ output values). unfortunately it
mostly works for moderate chromaticity.

consider this image under blue led illumination, with the standard D65 matrix
applied as IDT:

<img style="width:100%" src="out-of-gamut-warm.jpg"/>

you'll notice how the more extreme blues are pushed way out of spectral locus
in the CIE xy chromaticity chart in the top right corner.

we can try to white balance this with more matrix math (CAT16):

<img style="width:100%" src="out-of-gamut-cold.jpg"/>

with, let's say, mediocre success.
the outcome: high-chroma values will be clamped and result in flat,
constant-colour, even noise-free regions. very distracting.

note that the input data from camera was good before we applied the matrix.
here is the camera rgb buffer re-interpreted as bt2020:

<img style="width:100%" src="out-of-gamut-camera-rgb.jpg"/>

this means we actively broke the data during IDT. the obvious wishlist:
all values inside spectral locus! white balance without destructive matrices!


## matrix as input device transform

this is the "matrix" approach in the results below.
since spectral sensitivity data is not always available, this is the
default code path today.

graph wiring:

![](graph-matrix.jpg)

parameters: set
```
colour:01:matrix to image
filmsim:01:input to bt2020
```
inputs: connect
```
spectra-em.lut to filmsim:01:spectra
```

the spektrafilm upsampling table is extended smoothly to imaginary colours
(negative spectral energy). it does *something* for extreme chroma, but likely
collapses some of these to very similar spectra, much like clamping.



## colour lut from spectral sensitivity functions as input device transform

this is the "clut" approach in the results section.

consider the Canon EOS 5D Mark II's response to MacAdam style box spectra:

![](macadam-5dm2.png)

this plots camera rgb r+g as x axis, and r+g+b (some measure of overall spectral energy) as y axis.
for better display, the colour is the rising-edge wavelength of the box spectra.

especially see the red bulge to the left where a chromaticity coordinate of (r,g) (x-axis) can be
reached by two different box spectra (points on the y-axis above each other).

this particular kind of metameric failure we don't have with the CIE observer,
and it results in some ambiguity in the mapping from camera rgb to cie observer space.

note that all conventionally used colour spaces (xyz, srgb, prophotorgb,
adobergb, bt2020, etc..) are metameric to the observer, i.e. a simple linear
transform away (multiply a matrix, and that is the *definition* of the
transform, no errors introduced, no additional metamers collapsed).

the colour lookup table (clut) approach is accurate where it is (and disregards
possible other metamers completely). it maps each camera rgb coordinate to exactly one
XYZ coordinate, where both may have been the result of the same spectrum as stimulus.

assumption: need to know ssf

approximation: assume a certain function space of likely spectra that go with observer XYZ coordinates.


graph wiring remains much as above:

![](graph-clut.jpg)

set
```
colour:01:matrix to clut
filmsim:01:input to bt2020
```
inputs: connect
```
spectra-em.lut to filmsim:01:spectra
${maker} ${model}.lut to colour:01:clut
```
(see the `clut.pst` preset)

use the `clut.pst` preset and a custom `.lut` file as input to the `colour`
module, see [docs about input device transform](../colour-input/readme.md).
no out of gamut, but suffer metameric failure *twice* by daisy-chaining
spectrum to camera rgb to observer space.


## direct spectral upsampling of camera rgb

this is the "direct" approach.

graph wiring:

![](graph-direct.jpg)

i.e. *no* `colour` module in the loop

wire a spectral upsampling lut to the `filmsim` module that is created specifically for your camera:
```
cd src/
./mkspectra tools/clut/data/Canon_EOS_5D_Mark_II
```
and if you don't have an ssf, dream one up for instance via `vkdt mkssf`.

set
```
filmsim:01:input to camera rgb
```
and connect
```
${maker} ${model}-em.lut to filmsim:01:spectra
```
(see the `spektrafilm-direct.pst` preset)

this approach also suffers from metameric failure, but only once and only the
unavoidable one by the camera. no out of gamut madness.


## comparing the three methods

Canon EOS 5D Mark II, measured ssf:
<table>
<tr>
<td><img src="img_0000.jpg"/></td>
<td><img src="img_0001.jpg"/></td>
<td><img src="img_0002.jpg"/></td>
</tr>
<tr>
<td><img src="img_0003.jpg"/></td>
<td><img src="img_0004.jpg"/></td>
<td><img src="img_0005.jpg"/></td>
</tr>
<tr>
<td><img src="img_0009.jpg"/></td>
<td><img src="img_0010.jpg"/></td>
<td><img src="img_0011.jpg"/></td>
</tr>
</table>

Panasonic DC-S9, ssf guessed by mkssf:
<table>
<tr>
<td><a href="img_0006.jpg"><img src="img_0006-small.jpg"/></a></td>
<td><a href="img_0007.jpg"><img src="img_0007-small.jpg"/></a></td>
<td><a href="img_0008.jpg"><img src="img_0008-small.jpg"/></a></td>
</tr>
</table>
images: mine, except portrait of woman: signatureedits.com free-raw-photos

## setup cheat sheet

### matrix
* `denoise` -> `colour` -> `filmsim`
* `param:colour:matrix:image`
* `param:filmsim:input:bt2020`
* `spectra-em.lut` -> `filmsim`

### clut
* `denoise` -> `colour` -> `filmsim`
* `param:colour:matrix:clut`
* `param:filmsim:input:bt2020`
* `spectra-em.lut` -> `filmsim`
* lut specific to camera -> `colour`

### direct
* `denoise` -> `filmsim`
* `param:filmsim:input:camera rgb`
* spectra lut specific to camera model -> `filmsim`
