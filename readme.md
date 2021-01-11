# vkdt: darktable which sucks less

this is an experimental complete rewrite of [darktable](https://darktable.org),
naturally at this point with a heavily reduced feature set.

the processing pipeline has been rewritten as a generic node graph (dag) which
supports multiple inputs and multiple outputs. all processing is done in glsl
shaders/vulkan. this facilitates potentially heavy duty computational
photography tasks, for instance aligning multiple raw files and merging them
before further processing, as well as outputting many intermediate results as
jpg for debugging. the gui profits from this scheme as well and can display
textures while they are still on gpu and output the data to multiple
targets, such as the main view and histograms.

## features

* very fast [GPU only](src/qvk/readme.md) processing
* [general DAG of processing operations](src/pipe/readme.md), featuring multiple inputs and outputs and
  feedback connectors for animation/iteration
* [full window colour management](doc/colourmanagement.md)
* [minimal set of image operation modules](src/pipe/modules/readme.md)
* [noise profiling](doc/noiseprofiling.md)
* [minimally invasive image database](src/db/readme.md)
* [command line utility](src/cli/readme.md)
* [real time magic lantern raw video (mlv) processing](src/pipe/modules/i-mlv/readme.md)

## build instructions

you should have checked out this repo recursively. if not, do
```
  git submodule init
  git submodule update
```

to grab the dependencies in the `ext/` folder. you should then be able to
simply run 'make' from the bin/ folder. for debug builds (which enable the
vulkan validation layers, so you need to have them installed), try
```
  cd bin/
  make debug -j12
```

simply run `make` without the `debug` for a release build. `make sanitize` is
supported to switch on the address sanitizer. changes to the compiled used can
be set in `config.mk`. if you don't have that file yet, copy it from
`config.mk.example`.

## running

the binaries are currently wired to run from the `bin/` directory:
```
  cd bin/
  ./vkdt -d all /path/to/your/rawfile.raw
```
raw files will be assigned the `bin/default-darkroom.cfg` processing graph.
if you run the command line interface 'vkdt-cli', it will replace all 'display'
nodes by 'export' nodes.
there are also a few example config files in `bin/examples/`. note that you
have to edit the filename in the example cfg to point to a file that actually
exists on your system.

## licence

our code is licenced under the 2-clause bsd licence (if not clearly marked
otherwise in the respective source files). there are parts from other libraries
that are licenced differently. in particular:

rawspeed:     LGPLv2
imgui:        MIT

and we may link to some others, too.

## dependencies
* vulkan, glslangValidator (libvulkan-dev, glslang-tools, or use the sdk)
* glfw (libglfw3-dev)
* submodule imgui
* submodule rawspeed (depends on pugixml, stdc++, zlib, jpeg, libomp-dev)
* libjpeg
* build: make, pkg-config

optional (configure in `bin/config.mk`):

* freetype (libfreetype-dev)
* exiv2 (libexiv2-dev)


## faq
* **can i run my super long running kernel without timeout?**  
if you're using your only gpu in the system, you'll need to run without xorg,
straight from a tty console. this means you'll only be able to use the
command line interface `vkdt-cli`. we force a timeout, too, but it's
something like 16 minutes. let us know if you run into this..

* **how do i build a binary package?**  
you mostly need the `bin/` directory for this. after running `make` inside
`bin/`, a straight copy of `bin/` to say `/opt/vkdt/` would work (you can put a
symlink to the binaries `vkdt` and `vkdt-cli` in `/usr/bin`).
the shader sources (`*.{glsl,comp,vert,geom,frag,tese,tesc}`) as well as the
`examples/` and various `test/` directories are optional and do not have to be
copied.
to build for generic instruction sets, be sure to edit `config.mk`, especially
set `OPT_CFLAGS=` to `-march=generic` and whatever you require. to convince
rawspeed to do the same, set `RAWSPEED_PACKAGE_BUILD=1`.

