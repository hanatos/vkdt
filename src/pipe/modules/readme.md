# iop API

we want these to be as independent of anything as we can. the idea would be to
arrive at a completely reusable pipeline with modules that can easily be linked
into the core as well as reused by other projects if they decide to do so. one
of our immediate use cases is gui frontend and cli which both want to use the
modular pipeline.

thus, we want to limit the calls from here back into the core, ideally avoiding
to link back into a core dso.

one idea, if needed, is that we'll provide an explicit subset of helper functions
as function pointers in a struct that is available to the module.

since modules do their core work in compute shaders, most simple technical helpers
will likely be short functions in glsl headers to be included and inlined.


# note on size negotiations:

preliminaries, to be defined in initialisation phase:
* need to look at raw/input file to get input pixel dimensions
* need to know output roi
* walk over source and sink nodes only

for each output (if many): pull data through graph:
* given output roi, compute input roi + size of
  required scratch pad memory
* topological sort, do depth first graph expansion?
* run through whole graph, keep track of mem requirements:
  * scratch pad mem
  * intermediate inputs at bifurcations
* iterate the above if mem exceeded: split output roi in two, keep
  track of other roi that need to be processed in the end

given all roi and max mem requirements met:
* memory management: reuse scratch pad mem and multiple input buffers
* construct command buffer by running through graph again
* modules may push back varying number of kernel calls depending
  on size
* for tiling and small roi we need a preview buffer run to be 
  interleaved with the regular roi processing. (cache these or just
  reprocess for every tile? usually in dr mode it'll be one tile/roi
  only)


modules can have multiple inputs, but only one output to pull on for roi
computations (scratchmem buffers may change sizes as a side effect).

TODO: though for debugging we can assign scratchmem buffers to a sink node.

TODO: maybe the colour picker and histogram are sink nodes, too? do
we then compute these as side effects or do they actively pull?
this would complicate memory management somewhat.
in dt, these are driven by preview pipe buffers only.

we don't want cycles obviously.

# files to describe a module
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

# list of modules
TODO: reference all the readme.md in the lower levels:

[bc1 input](./i-bc1/readme.md)
[thumbnails](./thumb/readme.md)
[highlight reconstruction](./hilite/readme.md)
[zone system](./zones/readme.md)
[blend](./blend/readme.md)
[bc1 output](./o-bc1/readme.md)
[alignment](./burst/readme.md)
