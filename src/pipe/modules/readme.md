# modules in the image processing graph

image processing is done in a DAG with potentially multiple sources and multiple sinks.
the graph does in fact not strictly have to be acyclic, we allow `feedback` connectors
for iterative/multi-frame execution.

## list of modules
it follows the current list of available modules.

**input modules**

* [i-bc1: input module for bc1-compressed thumbnails](./i-bc1/readme.md)
* [i-geo: input binary 3d geometry as ssbo connector](./i-geo/readme.md)
* [i-jpg: jpg input module](./i-jpg/readme.md)
* [i-jpglst: input a longer list of jpg as an array connector](./i-jpglst/readme.md)
* [i-lut: half float lut input module](./i-lut/readme.md)
* [i-mlv: magic lantern raw video input module](./i-mlv/readme.md)
* [i-pfm: 32-bit floating point map input module](./i-pfm/readme.md)
* [i-quake: render the quake game](./i-quake/readme.md)
* [i-raw: rawspeed input module for photographic stills](./i-raw/readme.md)
* [i-v4l2: webcam input module](./i-v4l2/readme.md)
* [i-vid: video input module](./i-vid/readme.md)

**output modules**

* [o-bc1: write bc1 compressed thumbnail files](./o-bc1/readme.md)
* [o-ffmpeg: write h264 compressed video stream for multi-frame input](./o-ffmpeg/readme.md)
* [o-jpg: write jpeg compressed still image](./o-jpg/readme.md)
* [o-null: write absolutely nothing](./o-null/readme.md)
* [o-pfm: write uncompressed 32-bit floating point image](./o-pfm/readme.md)
* [loss: compute loss for optimisation](./loss/readme.md)

**visualisation and inspection modules**

* [ab: a/b images in split screen](./ab/readme.md)
* [check: mark out of gamut and under- and overexposure](./check/readme.md)
* [ciediag: vectorscope diagram in cie chromaticity space](./ciediag/readme.md)
* [display: generic display sink node](./display/readme.md)
* [hist: waveform histogram](./hist/readme.md)
* [pick: colour picker and visualisation tool](./pick/readme.md)
* [rawhist: raw histogram with estimated noise levels](./rawhist/readme.md)
* [test10b: render a gradient prone to banding to test 10 bit displays and dithering](./test10b/readme.md)
* [y2srgb: visualise first channel in grey scale](./y2srgb/readme.md)

**internal use**

* [nprof: create noise profile](./nprof/readme.md)
* [thumb: special display for bc1 thumbnails](./thumb/readme.md)

**processing modules**

* [accum: accumulate frames](./accum/readme.md)
* [align: align animation frames or burst photographs](./align/readme.md)
* [blend: masked frame blending](./blend/readme.md)
* [colour: generic colour manipulation/input transform](./colour/readme.md)
* [contrast: local contrast enhancement using the guided filter](./contrast/readme.md)
* [crop: crop/rotate/perspective correction](./crop/readme.md)
* [cnn: convolutional neural network](./cnn/readme.md)
* [deconv: deconvolution sharpening](./deconv/readme.md)
* [demosaic: demosaic bayer or x-trans raw files](./demosaic/readme.md)
* [denoise: noise reduction based on edge-aware wavelets and noise profiles](./denoise/readme.md)
* [draw: draw raster masks via brush strokes (e.g. for dodging and burning)](./draw/readme.md)
* [exposure: simple exposure correction, useful for dodging/burning](./exposure/readme.md)
* [f2srgb: convert linear floating point data to 8-bit sRGB for output](./f2srgb/readme.md)
* [filmcurv: parametric log + contrast S shaper curve](./filmcurv/readme.md)
* [filmsim: dummy for future implementation of analog film simulation](./filmsim/readme.md)
* [grad: linear gradient density filter](./grad/readme.md)
* [grade: simple ACES CDL grading tool](./grade/readme.md)
* [guided: guided filter blur module, useful for refining drawn masks](./guided/readme.md)
* [hilite: highlight reconstruction based on local inpainting](./hilite/readme.md)
* [inpaint: smooth reconstruction of masked out areas](./inpaint/readme.md)
* [lens: lens distortion correction](./lens/readme.md)
* [llap: local contrast, shadow lifting, and highligh compression via local laplacian pyramids](./llap/readme.md)
* [resize: add ability to resize buffers](./resize/readme.md)
* [saturate: simple rgb saturation](./saturate/readme.md)
* [spec: spectral colour manipulation/input transform](./spec/readme.md)
* [srgb2f: convert sRGB input to linear rec2020 floating point](./srgb2f/readme.md)
* [vignette: add/remove parametric vignette](./vignette/readme.md)
* [wavelet: skin retouching](./wavelet/readme.md)
* [zones: zone system-like tone manipulation tool](./zones/readme.md)

**3d rendering**

* [rt: real-time ray tracing](./rt/readme.md)
* [spheres: shadertoy demo ported for testing](./spheres/readme.md)
* [sss: sub surface scattering testbed](./sss/readme.md)


## default pipeline

by default, a raw image is passed through the following pipeline:

* `denoise`: (this module also subtracts the black point and removes black borders which are otherwise used for noise estimation)
* `hilite`: reconstruct highlights on raw data
* `demosaic`: interpolate three colour channels for every pixel
* `crop`: grab exif orientation and allow crop/perspective correction
* `colour`: apply white balance, colour matrix, and gamut corrections. optionally apply fully profiled RBF for colour correction.
* `filmcurv`: film style tone curve
* `llap`: local contrast, shadows, and highlights

you can change the default pipeline by hacking `bin/default-darkroom.i-raw` for
darkroom mode and `bin/default.i-raw` for thumbnails. the `i-raw` suffix
indicates that the file will be used for raw input, there is also the
equivalent `i-mlv` version for raw video.


## files to describe a module

* `flat.mk` is used to express compile time dependencies (if you're including glsl files or if you have a main.c to be compiled)
* `connectors` defines the io connectors, for instance
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

format can be `ui8` `ui16` `ui32` `f16` `f32`.

when connecting two modules, the connectors will be tested for compatibility.
some modules can handle more than one specific configuration of channels and
formats, so they specify a wildcard `*` instead. if both reader and writer
specify `*` vkdt defaults to `rgba:f16`.

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

* `params` defines the parameters that can be set in the `cfg` files and which
  will be routed to the compute shaders as uniforms. for instance
```
sigma:float:1:0.12
shadows:float:1:1.0
hilights:float:1:1.0
clarity:float:1:0.0
```
* `params.ui` defines the mapping of parameters to ui widgets. i recommend you
  look through existing examples to get a sense. the ui is programmed in c++,
  the modules in c, and the processing is done in glsl. this way there is an
  extremely clear separation of algorithms, module logic, and gui.

note that most strings here (parameter and connector names etc) are stored as
`dt_token_t`, which is exactly 8 bytes long. this means we can very easily
express the parameters and corresponding history stacks in a binary format,
which is useful for larger parameter sets such as used for the vertices
in the `draw` module.

