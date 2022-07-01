# utilities to create input device transforms

this page is about characterising the colour of an input device. there is
[a separate page for colour management on the display device](../../../doc/colourmanagement.md).

to generate very accurate input colour transforms from camera rgb to profiled
rec2020, `vkdt` supports colour lookup tables as a replacement for the often
used 3x3 matrices.

the resulting *colour lookup tables* (clut) contain two tables, one adapted for
a tungsten illuminant (A) and one for daylight (D65). these can be
interpolated run time (much like the matrices/hsl tables in the dng spec) and
will never generate colour coordinates outside of the spectral locus.

ideally we will construct an input device transform from the spectral sensor
response of the camera. if you have such data, create a plain text file with it
in 380..780nm in 5nm intervals. this can be used with the `mkclut` tool to
create a colour lookup table from raw camera rgb to profiled rec2020.
in the `data/` subdirectory here are a few examples from the
[`rawtoaces` project](https://github.com/AcademySoftwareFoundation/rawtoaces).
these are also used
to construct a low dimensional model of camera response functions using a
principal component analysis (see Jun Jiang, Dengyu Liu, Jinwei Gu, Sabine
Süsstrunk *What is the space of spectral sensitivity functions for digital
color cameras?* 2013).

this model is used in the `mkssf` tool to estimate the ssf of your camera in
case you don't have measurements (probably the regular case). as additional
input to `mkssf` you can either extract a dng profile from a dng (created by
the adobe dng converter) or use two shots of a profiled colour checker target.
since we want to reconstruct the spectral response it is important to use a
target with known spectral reflectivity (the ColorChecker sold by
xrite/calibrite).


## mkssf: estimating spectral response

estimate the *spectral sensitivity functions (ssf)* of a camera.
this tool has two modes of operation. it can output ssf

* by reverse-fitting them to dng profiles (`.dcp`)
* by matching two images of ColorChecker colour charts (under two illuminants).

it will create an html report along with the ssf data to check the accuracy of
the results.

### usage

```
mkssf <dng|dcp file>        dng or dcp file containing profile data
      --picked <a> <d65>    work with images of the cc24 chart instead
```

### examples

```
mkssf IMG_0001.dng
```

will output `<camera model>.txt` the ssf, `<camera model>.html` the report, and
`<camera model>.png` a plot of the ssf used in the report.


### getting a dng/dcp

we read the following tags as input, using `exiftool`:

```
UniqueCameraModel ColorMatrix[123] CameraCalibration[123] AnalogBalance
ReductionMatrix[123] ForwardMatrix[123] ProfilehueSatmapDims
ProfileHueSatMapData[123]
```

you can use the adobe dng creator to convert your raw files to dng. it will
fill the required tags into the exif fields. alternatively, if you have an
installation of an adobe tool, the `.dcp` files are understood by `exiftool`
as well. they can be found in a directory like `/ProgramData/Adobe/CameraRaw/CameraProfiles/Camera/`.


### taking pictures of a chart

we require two images as input: one lit by D65 (daylight) and one by
illuminant A (incandescent). the usual wisdom about avoiding glare and looking
for uniform illumination, avoiding vignetting by not filling the whole frame etc
applies here.

preparing the images for `mkssf`:

to generate input for `mkssf`, load the images in `vkdt`, rectify them using
the perspective correction in the `crop` module, and crop them so only the
rectified patches are visible.

use the colour picker `cc24.pst` preset to pick all spots at once. connect the
`pick` module to the output of the `crop` module, such that it will see camera
rgb values (i.e. in particular connect it *before* the `colour` module).

if you now exit darkroom mode, the `.cfg` of the file will be written and will
contain a line where the parameters of the `pick` module are stored and contain
the picked colours in an array:

```
param:pick:01:picked:0.0248955:0.0332774:0.0206327:0.0891787:0.119617:0.0793049:0.0343796:0.0967884:0.0993389:0.0212565:0.0501072:0.0231642:0.049632:0.111285:0.12099:0.0532441:0.183111:0.135137:0.0952556:0.0760156:0.0223686:0.0215014:0.0689848:0.102514:0.079723:0.0611236:0.0446295:0.0176179:0.0283354:0.0361582:0.0688749:0.161514:0.0511192:0.105295:0.121057:0.0314682:0.0111977:0.0411988:0.0713408:0.0294649:0.102019:0.0442576:0.0570832:0.0308679:0.0174226:0.133268:0.197586:0.0469638:0.0779897:0.0750722:0.084151:0.0305505:0.125637:0.128063:0.177048:0.368113:0.27652:0.120213:0.254218:0.195014:0.0747832:0.160176:0.123711:0.0387066:0.0814506:0.0626062:0.0179179:0.0378751:0.0295504:0.0059353:0.012358:0.00960426
```

for each of the two illuminants, create a `.txt` file that contains only
this line (say `picked_a.txt` and `picked_d65.txt`).



## mkclut: generating a colour lut profile

create a lookup table to be used as an input device transform by `vkdt`. it
will convert camera rgb values to rec2020 coordinates. takes as input the known
spectral sensitivity functions (ssf) of the cameras colour filter array (cfa).
by default it will create a transform that is computed for illuminant A and for
illuminant D65, such that these can be interpolated run time (similar to
matrices in the dng pipeline).

### usage:

```
mkclut <model>            model.txt will be openend as cfa data,
                          model.lut will be written as output.
       --illum0 <illum>   optional spectral illuminant description, txt extension will be added.
       --illum1 <illum>   spectral characterisation of second illuminant
```

### examples:

```
./mkclut <camera model>.txt
```

will output `<camera model>.pfm` for visual inspection as well as `<camera model>.lut`
the actual colour lookup table to be used in `vkdt`.
copy this to the `data/` directory of the `vkdt` install, so the program will pick it
up when searching for `data/${model}.lut`. this can be conveniently routed into
[the `colour` module](../../pipe/modules/colour/readme.md) by applying the `clut.pst` preset.
