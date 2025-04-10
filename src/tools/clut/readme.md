# utilities to create input device transforms

this page is about characterising the colour of an input device. there is
[a separate page for colour management on the display device](../../../doc/howto/colour-display/readme.md).

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

this model is used in the `vkdt-mkssf` tool to estimate the ssf of your camera in
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

for instance, here is the output html generated for the Canon EOS 5D Mark II when retrofitting spectral
responses with a gaussian mixture model to the adobe dcp:

<div style='width:90%'>
<h2>A | CIE | D65</h2>
<p>illuminant A + cfa + dng profile, illuminant D65 + cfa + dng profile, vs. ground truth cie observer in the middle</p>
<table style='border-spacing:0;border-collapse:collapse'><tr>
<td style='background-color:rgb(117.429,84.1436,63.5622);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(121.945,81.8087,59.4364);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(125.03,84.559,61.4365);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(199.635,143.998,118.415);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(206.628,144.169,109.162);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(208.813,146.834,113.948);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(99.3122,110.098,126.788);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(103.231,120.48,134.663);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(114.712,111.265,136.647);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(72.0031,114.594,57.8959);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(98.6601,106.413,55.9946);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(87.8964,118.144,50.743);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(144.612,112.184,143.832);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(139.751,126.136,150.542);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(154.265,112.196,154.108);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(84.9685,181.74,144.416);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(115.033,185.395,146.254);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(120.683,188.202,146.541);width:33px;height:100px;padding-left:0px'> </td></tr><tr>
<td style='background-color:rgb(221.645,132.228,66.2735);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(229.656,121.007,32.1355);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(235.778,129.778,44.5459);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(101.546,63.6554,135.016);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(81.2614,92.1554,145.134);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(110.259,57.6949,149.203);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(197.178,87.077,93.0465);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(203.855,82.8412,82.9872);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(198.16,85.9446,89.7772);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(111.969,52.6214,87.018);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(97.5931,61.8358,90.1049);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(109.029,52.0824,91.8398);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(132.869,197.234,70.2191);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(171.08,183.869,47.316);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(163.018,203.308,17.7348);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(219.848,173.498,64.0193);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(238.909,156.982,13.9597);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(240.718,173.527,7.91466);width:33px;height:100px;padding-left:0px'> </td></tr><tr>
<td style='background-color:rgb(75.3085,10.3978,117.659);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(52.2107,65.7261,127.392);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(84.5961,0,131.922);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(0,151.283,69.7915);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(86.4913,145.046,61.6956);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(66.8125,158.107,52.7068);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(184.457,59.5984,75.1028);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(184.597,49.7103,49.3719);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(175.106,63.9034,59.7831);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(221.661,212.936,67.4876);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(250.753,194.663,0);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(246.164,216.885,0);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(201.964,73.2537,128.542);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(197.903,83.7483,128.104);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(198.154,71.0284,133.348);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(15.7781,114.883,139.026);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(0,132.769,144.212);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(48.8786,120.36,147.656);width:33px;height:100px;padding-left:0px'> </td></tr><tr>
<td style='background-color:rgb(241.872,237.3,201.967);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(261.512,240.157,205.184);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(272.255,240.456,208.394);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(196.575,194.676,167.523);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(213.127,197.343,171.43);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(222.306,197.069,173.73);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(156.939,156.001,134.243);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(170.617,158.09,137.699);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(177.943,157.828,139.432);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(117.567,117.486,101.123);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(128.011,119.104,103.847);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(133.564,118.854,105.116);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(83.3156,83.4437,72.1407);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(90.6376,84.6604,74.1946);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(94.6841,84.41,75.0623);width:33px;height:100px;padding-left:0px'> </td><td style='background-color:rgb(53.4619,52.3567,45.4855);width:33px;height:100px;padding-right:0px'> </td><td style='background-color:rgb(57.7926,53.0941,46.7521);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td><td style='background-color:rgb(60.2706,52.9689,47.2644);width:33px;height:100px;padding-left:0px'> </td></tr><tr>
</tr></table></div>
<div style='width:80%'>
<h2>estimated cfa model gauss, 30 coeffs</h2>
<img style="width:100%;" src='Canon EOS 5D Mark II.png'/><p>residual loss 0.00317613</p></div>
<br/>

<div style='width:80%'>
<h2>reference from rawtoaces</h2>
<img style="width:100%;" src='reference.png'/></div>
<br/>
which is a bit different in spectral shape, so take the results with a grain of salt.
the overall relation between the three is similar and the location of the peaks is comparable.
a spectral lut created from these will likely look okay and importantly avoid the situation
where some colours are pushed outside the spectral locus.


### usage

```
vkdt mkssf <dng|dcp file>        dng or dcp file containing profile data
           --picked <a> <d65>    work with images of the cc24 chart instead
```

### examples

```
vkdt mkssf IMG_0001.dng
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

preparing the images for `vkdt mkssf`:

to generate input for `vkdt mkssf`, load the images in `vkdt`, rectify them using
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



## vkdt-mkclut: generating a colour lut profile

create a lookup table to be used as an input device transform by `vkdt`. it
will convert camera rgb values to rec2020 coordinates. takes as input the known
spectral sensitivity functions (ssf) of the cameras colour filter array (cfa).
by default it will create a transform that is computed for illuminant A and for
illuminant D65, such that these can be interpolated run time (similar to
matrices in the dng pipeline).

### usage:

```
vkdt mkclut <model>            model.txt will be openend as cfa data,
                               model.lut will be written as output.
            --illum0 <illum>   optional spectral illuminant description, txt extension will be added.
            --illum1 <illum>   spectral characterisation of second illuminant
```

### examples:

```
vkdt mkclut <camera model>.txt
```

will output `<camera model>.pfm` for visual inspection as well as `<camera model>.lut`
the actual colour lookup table to be used in `vkdt`.
copy this to the `data/` directory of the `vkdt` install, so the program will pick it
up when searching for `data/${model}.lut`. this can be conveniently routed into
[the `colour` module](../../pipe/modules/colour/readme.md) by applying the `clut.pst` preset.
