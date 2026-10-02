# spectral colorimetry

reference mkssf
reference input device transform

## the problems

first: out of spectral locus colour coordinates after transform:

![](out-of-gamut-warm.jpg)
![](out-of-gamut-cold.jpg)

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

the spektrafilm upsampling table is extended smoothly to imaginary colours (negative spectral energy). it does *something*:

graph wiring:
![](graph-matrix.jpg)

parameters:
set
```
colour:01:matrix to image
filmsim:01:input to bt2020
```


## spectral lut as input device transform

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


## results

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
