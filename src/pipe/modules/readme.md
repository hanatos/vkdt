# module reference documentation

image processing is done in a DAG with potentially multiple sources and multiple sinks.
the graph does in fact not strictly have to be acyclic, we allow `feedback` connectors
for iterative/multi-frame execution.

## list of modules
it follows the reference documentation of available modules, grouped into categories.
also see the [list of presets shipped with vkdt](../../../doc/howto/presets/presets.md).

**input**

* [i-bc1: input module for bc1-compressed thumbnails](./i-bc1/readme.md)
* [i-exr: input openexr images](./i-exr/readme.md)
* [i-hdr: greg ward's rgbe hdr images](./i-hdr/readme.md)
* [i-jpg: jpg input module](./i-jpg/readme.md)
* [i-jpglst: input a longer list of jpg as an array connector](./i-jpglst/readme.md)
* [i-lut: half float lut input module](./i-lut/readme.md)
* [i-mcraw: read motioncam raw video](./i-mcraw/readme.md)
* [i-mlv: magic lantern raw video input module](./i-mlv/readme.md)
* [i-obj: load triangle meshes from wavefront obj files](./i-obj/readme.md)
* [i-pfm: 32-bit floating point map input module](./i-pfm/readme.md)
* [i-raw: input module for raw-format photographic stills or timelapses](./i-raw/readme.md)
* [i-v4l2: webcam input module](./i-v4l2/readme.md)
* [i-vid: video input module](./i-vid/readme.md)

**output**

* [o-bc1: write bc1 compressed thumbnail files](./o-bc1/readme.md)
* [o-copy: copy the input file to a new destination](./o-copy/readme.md)
* [o-exr: write openexr image files](./o-exr/readme.md)
* [o-jpg: write jpeg compressed still image](./o-jpg/readme.md)
* [o-lut: write varying precision multi channel luts](./o-lut/readme.md)
* [o-null: write absolutely nothing](./o-null/readme.md)
* [o-pfm: write uncompressed 32-bit floating point image](./o-pfm/readme.md)
* [o-vid: write h264 or prores compressed video streams](./o-vid/readme.md)
* [o-web: transitional module for jpg and mp4 for webpages](./o-web/readme.md)
* [loss: compute loss for optimisation](./loss/readme.md)

**visualisation and inspection**

* [ab: a/b images in split screen](./ab/readme.md)
* [check: mark out of gamut and under- and overexposure](./check/readme.md)
* [ciediag: vectorscope diagram in cie chromaticity space](./ciediag/readme.md)
* [coltgt: render a colorchecker target for verification of display colour management setup](./coltgt/readme.md)
* [display: generic display sink node](./display/readme.md)
* [hist: waveform histogram](./hist/readme.md)
* [pick: colour picker and visualisation tool](./pick/readme.md)
* [rawhist: raw histogram with estimated noise levels](./rawhist/readme.md)
* [test10b: render a gradient prone to banding to test 10 bit displays and dithering](./test10b/readme.md)
* [vis: convert linear input to srgb colour ramp for visualisation](./vis/readme.md)

**raw processing**

* [ca: correct chromatic aberrations](./ca/readme.md)
* [demosaic: demosaic bayer or x-trans raw files](./demosaic/readme.md)
* [hilite: highlight reconstruction based on local inpainting](./hilite/readme.md)
* [jddcnn: joint demosaicing and denoising via neural network](./jddcnn/readme.md)

**colour processing**

* [colenc: encode colour for colour managed output like adobeRGB, P3, etc](./colenc/readme.md)
* [colour: generic colour manipulation/input transform](./colour/readme.md)
* [curves: rgb and 3x3 ych curve widgets](./curves/readme.md)
* [grade: simple ACES CDL grading tool](./grade/readme.md)
* [filmsim: spectral analog film simulation](./filmsim/readme.md)

**corrective**

* [autoexp: smooth auto exposure of video sequences](./autoexp/readme.md)
* [crop: crop/rotate/perspective correction](./crop/readme.md)
* [deconv: deconvolution sharpening](./deconv/readme.md)
* [dehaze: compute depth map from hazy landscape image and remove haze](./dehaze/readme.md)
* [denoise: noise reduction based on edge-aware wavelets and noise profiles](./denoise/readme.md)
* [hotpx: remove impulse noise/stuck pixels](./hotpx/readme.md)
* [kpn: kernel prediction neural network for denoising](./kpn/readme.md)
* [lens: lens distortion correction](./lens/readme.md)
* [negative: invert film negatives](./negative/readme.md)
* [usm: unsharp masking sharpening](./usm/readme.md)

**tone**

* [contrast: local contrast enhancement using the guided filter](./contrast/readme.md)
* [eq: local contrast equaliser](./eq/readme.md)
* [exposure: simple exposure correction, useful for dodging/burning](./exposure/readme.md)
* [filmcurv: display transform curve](./filmcurv/readme.md)
* [grad: linear gradient density filter](./grad/readme.md)
* [llap: local contrast, shadow lifting, and highligh compression via local laplacian pyramids](./llap/readme.md)
* [OpenDRT: sophisticated display transform with some colour correction options](./OpenDRT/readme.md)
* [vignette: add/remove parametric vignette](./vignette/readme.md)
* [zones: zone system-like tone manipulation tool](./zones/readme.md)

**retouching**

* [draw: draw raster masks via brush strokes (e.g. for dodging and burning)](./draw/readme.md)
* [guided: guided filter blur module, useful for refining drawn masks](./guided/readme.md)
* [inpaint: smooth reconstruction of masked out areas](./inpaint/readme.md)
* [mask: create parametric masks from colour images for use with blending](./mask/readme.md)
* [wavelet: skin retouching](./wavelet/readme.md)

**effects**

* [frame: postcard-style decor border around the image](./frame/readme.md)
* [grain: simulate analog film grain](./grain/readme.md)

**technical**

* [align: align animation frames or burst photographs](./align/readme.md)
* [blend: masked frame blending](./blend/readme.md)
* [cnngenin: generate random input for neural network training](./cnngenin/readme.md)
* [format: change texture format (number of channels and data type)](./format/readme.md)
* [kpn-t: kernel prediction neural network for denoising, training](./kpn-t/readme.md)
* [mv2rot: estimate rotation + translation from motion vectors](./mv2rot/readme.md)
* [resize: add ability to resize buffers](./resize/readme.md)
* [resnet: gmic convolutional neural network](./resnet/readme.md)

**3d/rendering**

* [accum: accumulate frames in a frame buffer](./accum/readme.md)
* [bvh: append triangle mesh to ray tracing acceleration structure](./bvh/readme.md)
* [logo: render the animated vkdt icon](./logo/readme.md)
* [physarum: mesmerizing interactive particle simulation](./physarum/readme.md)
* [quake: the 1996 game ray traced based on QSS](./quake/readme.md)
* [rt: real-time ray tracing](./rt/readme.md)
* [spheres: shadertoy demo ported for testing](./spheres/readme.md)
* [svgf: spatiotemporal variance guided filtering](./svgf/readme.md)

**internal use**

* [nprof: create noise profile](./nprof/readme.md)
* [thumb: special display for bc1 thumbnails](./thumb/readme.md)

## default pipeline

by default, a raw image is passed through the following pipeline:

* `denoise`: (this module also subtracts the black point and removes black borders which are otherwise used for noise estimation)
* `hilite`: reconstruct highlights on raw data
* `demosaic`: interpolate three colour channels for every pixel
* `crop`: grab exif orientation and allow crop/perspective correction
* `colour`: apply white balance, colour matrix, and gamut corrections. optionally apply fully profiled RBF for colour correction.
* `filmcurv`: film style tone curve
* `llap`: local contrast, shadows, and highlights

you can change the default pipeline by hacking `default-darkroom.i-raw` (either
in the vkdt basedir or the homedir) for darkroom mode and `default.i-raw`
for thumbnails. the `i-raw` suffix indicates that the file will be used for raw
input, there is also the equivalent `i-mlv` version for raw video.


## files to describe a module

note that most strings here (parameter and connector names etc) are stored as
`dt_token_t`, which is exactly 8 bytes long. this means we can very easily
express the parameters and corresponding history stacks in a binary format,
which is useful for larger parameter sets such as used for the vertices
in the `draw` module.

###  `flat.mk`
is used to express compile time dependencies (if you're including glsl files or
if you have a main.c to be compiled). this is build-time only and not needed
to run `vkdt`.

### `connectors`
defines the io connectors, for instance
```
input:read:rgba:f16
output:write:rgba:f16
```
defines one connector called `input` and one called `output` in `rgba` with `f16` format.

the specifics of a connector are name, type, channels, and format. name is an
arbitrary identifier for your perusal. note however that `input` and `output`
trigger special conventions for default callbacks wrt region of interest or
propagation of image parameters (`module->img_param`).

the type is one of `read` `write` `source` `sink`. sources and sinks do not
have compute shaders associated with them, but will call `read_source` and
`write_sink` callbacks you can define in a custom `main.c` piece of code.

the channels can be anything you want, but the GPU only supports one, two, or
four channels per pixel. these are represented by one char each, and will be
matched during connection of modules.

format can be one of the primitive `ui8` `ui16` `ui32` `f16` `f32` or one of
the special formats `dspy` and `atom`. `dspy` evaluates to the display
capabilities (may be a 10 bits/channel special format). `atom` evaluates to
`f32` if supported, or else falls back to `ui32` (many amd cards).

when connecting two modules, the connectors will be tested for compatibility.
some modules can handle more than one specific configuration of channels and
formats, so they specify a wildcard `*` instead. if both reader and writer
specify `*` `vkdt` defaults to `rgba:f16`.

there is one more special case, modules can reference the channel or format of
a previously connected connector on the same module, for instance the blend module:
```
back:read:*:*
input:read:*:*
mask:read:y:f16
output:write:&input:f16
```
references the channel configuration of the `input` connector and configures
the `output` connector to match it. note that this requires to connect `input`
before `output`.

### `params`
defines the parameters that can be set in the `cfg` files and which
  will be routed to the compute shaders as uniforms. for instance
```
sigma:float:1:0.12
shadows:float:1:1.0
hilights:float:1:1.0
clarity:float:1:0.0
```
### `params.ui`
defines the mapping of parameters to ui widgets. i recommend you look through
existing examples to get a sense. the ui is programmed in c++, the modules in
c, and the processing is done in glsl. this way there is an extremely clear
separation of algorithms, module logic, and gui.

the format is in general, one per line:

`<name of parameter>:<widget>:<special info for widget>`

the ui supports the following widgets

* `slider` takes `min:max` as range argument
* `vslider` a vertical slider. takes `min:max` as range argument
* `callback` a special button that triggers the `ui_callback` function in the module
* `colour` 
* `combo` a combobox. takes the `:`-separated list of string entries as argument
* `crop` the crop tool of the crop/rotate module
* `draw` draw brush strokes with the mouse/pentablet
* `filename` a file name
* `grab` grab the keyboard and mouse and pass to the module
* `group` a special directive, see below
* `hidden` do not show the parameter in the ui
* `pers` the perspective correction tool of the crop/rotate module
* `pick` a special colour picker widget (use mouse to draw rect on image)
* `print` print a string using the parameter
* `rbmap` a special colour widget for radial basis function mapping
* `rgb` three sliders (red green blue) which show the colour in the ui background. takes the usual range argument `min:max`.
* `straight` the straighten tool of the crop/rotate module


the `group` keyword is used to only show the following ui elements if a
parameter switch is set accordingly. syntax: `group:<param>:<val>`. for an
example, see [the colour module](colour/readme.md). here, `<param>`
refers to an `int` parameter driven by a combo box, and we use it to only show
the colour temperature slider if the matrix mode is set to colour lookup table
`clut`:

```
matrix:combo:rec2020:image:XYZ:rec709:clut
group:matrix:4
temp:slider:2856:6504
```

everything after the `group` directive until the next one will be shown only if
the `matrix` parameter is set to `4`.

