# matrix-free image formation

in this article we show how to render an image from camera rgb without using matrices.

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


## input device transform recap

and how matrix transforms break stuff bad.

show blue images

"matrix" approach


## input transform from spectral sensitivity functions

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


## spektrafilm

implemented in the `filmsim` module.
spectral image processing via upsampling of the input.
supply upsampling table as input connector `spectra`. that is `xy` -> `c0 c1 c2 c3`

normally done by daisy-chaining input device transform and then upsampling from the observer space.
nice and general, but inherits the problems of the IDT:
* out of gamut values for matrices
* metameric failure is of the camera CFA *and* of the transform to observer coordinates XYZ.


## spectral upsampling from camera rgb directly

for spektrafilm (the `filmsim` module) only.

## comparing the three methods

graph setup:
TODO screenshots

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
