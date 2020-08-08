# modules for the image processing graph

image processing is done in a DAG with potentially multiple sources and multiple sinks.

## list of modules
TODO: reference all the readme.md in the lower levels:

* [alignment](./burst/readme.md)
* [bc1 input](./i-bc1/readme.md)
* [bc1 output](./o-bc1/readme.md)
* [blend](./blend/readme.md)
* [colour](./colour/readme.md)
* [crop and rotate](./crop/readme.md)
* [denoise](./denoise/readme.md)
* [film curve](./filmcurv/readme.md)
* [highlight reconstruction](./hilite/readme.md)
* [histogram (waveform)](./hist/readme.md)
* [histogram (raw)](./rawhist/readme.md)
* [local contrast, shadows, and highlights](./llap/readme.md)
* [pick colours](./pick/readme.md)
* [saturation](./saturate/readme.md)
* [test 10bit display](./test10b/readme.md)
* [thumbnails](./thumb/readme.md)
* [zone system](./zones/readme.md)


## default pipeline

by default, a raw image is passed through the following pipeline:

* denoise (dummy module which essentially just subtracts the black point and removes black borders which are otherwise used for noise estimation)
* hilite: reconstruct highlights on raw data
* demosaic: interpolate three colour channels for every pixel
* crop: grab exif orientation and allow crop/perspective correction
* colour: apply white balance, colour matrix, and gamut corrections. optionally apply fully profiled RBF for colour correction.
* filmcurv: film style tone curve
* llap: local contrast, shadows, and highlights

you can change the default pipeline by hacking `bin/default-darkroom.cfg` for darkroom mode
and `bin/default.cfg` for thumbnails.


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

