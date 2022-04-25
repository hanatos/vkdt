# local contrast, shadows, and highlights

## parameters

* `sigma` this controls the center region of the tonality, i.e. it determines what are mid tones
  and what are extremal values. increasing sigma will make more pixels be considered as mid tones.
  this can be useful if you mainly care about contrast in the image. it will make the `clarity`
  slider more powerful. decreasing `sigma` will remove from the area where `clarity` has a big effect
  and instead consider a larger range of pixels as *shadows* or *highlights*, giving more power
  to the corresponding sliders.
* `shadows` this setting is used to control the shadows of the image. pulling shadows to 0 means
  shadows will disappear. increasing the shadow value will pull more pixels into the shadows.
* `hilights` this is similar but for highlights. use it to control the sharpness of your
  speculars.
* `clarity` this controls the amount of local contrast. it can also be used to smooth the image
  if you dial in a negative number.

## technical

this employs local laplacian pyramids, with a few extra tweaks for improved
quality, and in a glsl implementation for speed.

it works on the luminance channel only (Y channel of XYZ), and reconstructs
colours later by applying the method of Mantiuk 2009.


