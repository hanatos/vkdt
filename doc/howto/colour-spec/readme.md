# spectral colorimetry, matrix-free

in this article we show how to render an image from camera rgb without using matrices.

reference mkssf
reference input device transform

## image formation in a camera device

TODO keep the double-dollars around
TODO include like this
```
 <link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/katex@0.18.9/dist/katex.min.css" integrity="sha384-lPx0C4zIUZLpveABMwOFcFeGZwsvKBJfhJ85FN1PYOV7xApBcFMhcAEMVKF8loOI" crossorigin="anonymous">

    <!-- The loading of KaTeX is deferred to speed up page rendering -->
    <script defer src="https://cdn.jsdelivr.net/npm/katex@0.18.9/dist/katex.min.js" integrity="sha384-19KE2cFb3U+RUWmyhBz7aLOGDG8WrRC6hE3oY/HTZZlAAVWYTdmvLC//+TIV3zUx" crossorigin="anonymous"></script>

    <!-- To automatically render math in text elements, include the auto-render extension: -->
    <script defer src="https://cdn.jsdelivr.net/npm/katex@0.18.9/dist/contrib/auto-render.min.js" integrity="sha384-bjyGPfbij8/NDKJhSGZNP/khQVgtHUE5exjm4Ydllo42FwIgYsdLO2lXGmRBf5Mz" crossorigin="anonymous"
        onload="renderMathInElement(document.body);"></script>
```
better to get a release from
```
https://github.com/KaTeX/KaTeX/releases
```
and see which of the files we wish to keep
does this work? $$\int_\Lambda P(\lambda) d\lambda$$


## spektrafilm

implemented in the `filmsim` module.
spectral image processing via upsampling of the input.
supply upsampling table as input connector `spectra`. that is `xy` -> `c0 c1 c2 c3`

normally done by daisy-chaining input device transform and then upsampling from the observer space.
nice and general, but inherits the problems of the IDT:
* out of gamut values for matrices
* metameric failure is of the camera CFA *and* of the transform to observer coordinates XYZ.



## input device transform recap

and how matrix transforms break stuff bad.

show blue images

"matrix" approach

first: out of spectral locus colour coordinates after transform:

![](out-of-gamut-warm.jpg)
![](out-of-gamut-cold.jpg)

outcome: these values will be clamped and result in flat, constant-colour, even noise-free regions. very distracting.

note that this data was good before we applied the matrix:
![](out-of-gamut-camera-rgb.jpg)

want: in gamut! white balance!

second: metameric failure, induced by observer

Canon EOS 5D Mark II, response to MacAdam style box spectra:
![](macadam-5dm2.png)
this plots camera rgb r+g as x axis, and r+g+b (some measure of overall spectral energy) as y axis.
for better display, the colour is the rising-edge wavelength of the box spectra.

especially see the red bulge to the left where a chromaticity coordinate of (r,g) (x-axis) can be
reached by two different box spectra (points on the y-axis above each other).

this particular kind of metameric failure we don't have with the CIE observer.


## matrix as input device transform

"matrix" approach

the spektrafilm upsampling table is extended smoothly to imaginary colours (negative spectral energy). it does *something*:

graph wiring:
![](graph-matrix.jpg)

parameters:
set
```
colour:01:matrix to image
filmsim:01:input to bt2020
```


## colour lut from spectral sensitivity functions as input device transform

"clut" approach

mention observer metamerism and how metameric failure results in some ambiguity
in the mapping camera rgb -> cie observer space. note that all conventionally
used colour spaces (xyz, srgb, prophotorgb, adobergb, bt2020, etc..) are
metameric to the observer, i.e. a simple linear transform away (multiply a
matrix, and that is the *definition* of the transform, no errors introduced, no
additional metamers collapsed).

this approach is accurate where it is (and disregards possible other metamers completely).

assumption: need to know ssf

approximation: assume a certain function space of likely spectra that go with observer xyz coordinates.


graph wiring:
![](graph-clut.jpg)

set
```
colour:01:matrix to clut
filmsim:01:input to bt2020
```

use the `clut.pst` preset and a custom `.lut` file as input to the `colour` module, see XXX

no out of gamut, but suffer metameric failure *twice* by daisy-chaining spectrum to camera rgb to observer space.


## direct spectral upsampling of camera rgb

"direct" approach

graph wiring:
![](graph-direct.jpg)

i.e. *no* `colour` module in the loop

wire a spectral upsampling lut to the `filmsim` module that is created specifically for your camera:
```
cd src/
./mkspectra tools/clut/data/Canon_EOS_5D_Mark_II
```

set `filmsim:01:input` to `camera rgb`

XXX see mkssf

suffer from metameric failure, but only once and only the one by the camera. no out of gamut madness.


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

## setup cheat sheet

### matrix
* `denoise` -> `colour` -> `filmsim`
* `colour:matrix:`image`
* `filmsim:input:bt2020`
* `spectra-em.lut` -> `filmsim`

### clut
* `denoise` -> `colour` -> `filmsim`
* `colour:matrix:clut`
* `filmsim:input:bt2020`
* `spectra-em.lut` -> `filmsim`
* lut specific to camera -> `colour`

### direct
* `denoise` -> `filmsim`
* `filmsim:input:camera rgb`
* spectra lut specific to camera model -> `filmsim`
