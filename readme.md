# raw image processing workflow which sucks less

[vkdt](https://jo.dreggn.org/vkdt) is a workflow tool for raw photography.
`vkdt` is designed with high performance in mind. it features a flexible
processing node graph at its core, enabling real-time support for animations,
timelapses, raw video, and heavy lifting algorithms like image alignment and
better highlight inpainting. this is made possible by faster processing,
allowing more complex operations.

the processing pipeline is a generic node graph (DAG) which
supports multiple inputs and multiple outputs. all processing is done in glsl
shaders/vulkan. this facilitates potentially heavy duty computational
photography tasks, for instance aligning multiple raw files and merging them
before further processing, as well as outputting intermediate results for
debugging. the gui profits from this scheme as well and can display
textures while they are still on GPU and output the data to multiple
targets, such as the main view and histograms.

## features

* [very fast GPU only processing](src/qvk/readme.md)
* [general DAG of processing operations](src/pipe/readme.md), featuring multiple inputs and outputs and
  feedback connectors for animation/iteration
* [full window colour management](doc/howto/colour-display/readme.md)
* [noise profiling](doc/howto/noise-profiling/readme.md)
* [minimally invasive folder-based image database](src/db/readme.md)
* [command line utility](src/cli/readme.md)
* [real time magic lantern raw video (mlv) processing](src/pipe/modules/i-mlv/readme.md)
* [10-bit display output](src/pipe/modules/test10b/readme.md)
* [gamepad input](https://github.com/ocornut/imgui/issues/787) inherited from imgui
* [automatic parameter optimisation](src/fit/readme.md), for instance to fit vignetting
* [heavy handed processing](src/pipe/modules/cnn/readme.md) at almost realistic speeds

## documentation

* [image operation modules documentation](src/pipe/modules/readme.md)
* [how to do selected things](doc/howto/howto.md)

## packages

there are up-to-date packages (deb/rpm/pkgbuild) in the

[opensuse build system](https://software.opensuse.org/download.html?project=graphics%3Adarktable%3Amaster&package=vkdt)

also there are [nixos packages](https://search.nixos.org/packages?channel=unstable&show=vkdt) `vkdt`
and `vkdt-wayland` which you can use to try out/run on any linux distro, for instance:
```
  nix-shell -p vkdt
```


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

simply run `make` without the `debug` for a release build. note that the debug
build includes some extra memory overhead and performs expensive checks and can
thus be much slower. `make sanitize` is supported to switch on the address
sanitizer. changes to the compile time configuration as well as the compiler
toolchain can be set in `config.mk`. if you don't have that file yet, you can
copy it from `config.mk.defaults`.

## running

the binaries are put into the `bin/` directory. if you want to run `vkdt` from
anywhere, create a symlink such as `/usr/local/bin/vkdt -> ~/vc/vkdt/bin/vkdt`.
```
  cd bin/
  ./vkdt -d all /path/to/your/rawfile.raw
```
raw files will be assigned the `bin/default-darkroom.i-raw` processing graph.
if you run the command line interface `vkdt-cli`, it will replace all `display`
nodes by export nodes.
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
* glfw (libglfw3-dev and libglfw3, only use libglfw3-wayland if you're on wayland)
* submodule imgui
* submodule rawspeed (depends on pugixml, stdc++, zlib, jpeg, libomp)
* libjpeg
* build: make, pkg-config, clang, rsync, sed
* build rawspeed: cmake, libomp-dev

optional (configure in `bin/config.mk`):

* freetype (libfreetype-dev libpng16-16) for nicer font rendering
* exiv2 (libexiv2-dev) for raw metadata loading to assign noise profiles
* asound (libasound2) for audio support in mlv raw video
* ffmpeg (libavformat-dev libavcodec-dev) for the video input module `i-vid`

you can also build without rawspeed if that is useful for you.


## faq
* **can i load canon cr3 files?**  
yes. the rawspeed submodule in this repository is now by default
already using a branch that supports cr3 files. essentially on top
of vanilla rawspeed, it is a merge of:
```
cd ext/rawspeed
git remote add cytrinox https://github.com/cytrinox/rawspeed.git
git fetch --all
git checkout canon_cr3
cd ../../
rm -rf built/ext
cd bin
make -j20
```

* **does it work with wayland?**  
`vkdt` has been confirmed to run on wayland, using amd hardware.
there are a few known quirks, such as fullscreen mode (f11) does not
work and there were refresh issues when window focus is lost.

* **can i run my super long running kernel without timeout?**  
if you're using your only gpu in the system, you'll need to run without xorg,
straight from a tty console. this means you'll only be able to use the
command line interface `vkdt-cli`. we force a timeout, too, but it's
something like 16 minutes. let us know if you run into this..

* **can i build without display server?**  
there is a `cli` target, i.e. you can try to run `make cli` to only generate
the command line interface tools that do not depend on xorg or wayland or glfw.

* **how do i build a binary package?**  
you mostly need the `bin/` directory for this. after running `make` inside
`bin/`, a straight copy of `bin/` to say `/opt/vkdt/` would work (you can put a
symlink to the binaries `vkdt` and `vkdt-cli` in `/usr/bin`).
the shader sources (`*.{glsl,comp,vert,geom,frag,tese,tesc}`) as well as the
`examples/` and various `test/` directories are optional and do not have to be
copied.
to build for generic instruction sets, be sure to edit `config.mk`, especially
set `OPT_CFLAGS=` to `-march=x86-64` (not `native`) and whatever you require.
to convince rawspeed to do the same, set `RAWSPEED_PACKAGE_BUILD=1`.

* **i have multiple GPUs and vkdt picks the wrong one by default. what do i do?**  
make sure the GPU you want to run has the HDMI/dp cable attached (or else you
can only run `vkdt-cli`) on it. then run `vkdt -d qvk` and find a line such as  
```
[qvk] dev 0: NVIDIA GeForce RTX 2070
```
and then place this printed name exactly as written there in your  
`~/.config/vkdt/config.rc` in a line such as, in this example:
```
strqvk/device_name:NVIDIA GeForce RTX 2070
```
if you have several identical models in your system, you can use the device
number `vkdt` assigns (an index starting at zero, again see the log output),
and instead use
```
intqvk/device_id:0
```

* **can i use my pentablet to draw masks in vkdt?**  
yes, but you need a specific version of glfw to support it.
you can for instance clone `https://github.com/hanatos/glfw`,
for instance to `/home/you/vc/glfw`, and then put the
following in your custom `bin/config.mk`:  
```
VKDT_GLFW_CFLAGS=-I/home/you/vc/glfw/include/
VKDT_GLFW_LDFLAGS=/home/you/vc/glfw/build/src/libglfw3.a
VKDT_USE_PENTABLET=1
export VKDT_USE_PENTABLET
```

* **are there system recommendations?**  
`vkdt` needs a vulkan capable GPU. it will work better with floating point atomics,
in particular `shaderImageFloat32AtomicAdd`. you can check this property for certain
devices [on this website](https://vulkan.gpuinfo.org).
also, `vkdt` requires 4GB video ram (it may run with less, but this seems to be a
number that is fun to work with). a fast ssd is desirable since disk io is often times
a limiting factor, especially during thumbnail creation.

* **can i speed up rendering on my 2012 on-board GPU?**  
you can set the level of detail (LOD) parameter in your
`~/.config/vkdt/config.rc` file: `intgui/lod:1`. set it to `2` to only render
exactly at the resolution of your screen (will slow down when you zoom in), or
to `3` and more to brute force downsample.

* **can i limit the frame rate to save power?**  
there is the `frame_limiter` option in `~/.config/vkdt/config.rc` for this.
set `intgui/frame_limiter:30` to have at most one redraw every `30` milliseconds.
leave it at `0` to redraw as quickly as possible.

* **where can i ask for support?**  
try `#vkdt` on `oftc.net` or ask on [pixls.us](https://discuss.pixls.us).
