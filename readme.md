# raw photography processing workflow
# which sucks less

[vkdt](https://jo.dreggn.org/vkdt/) is a workflow toolbox for raw stills and video.
`vkdt` is designed with high performance in mind. it features a flexible
processing node graph at its core, enabling real-time support for animations,
timelapses, raw video, and heavy lifting algorithms like image alignment and
better highlight inpainting. faster processing
allows us to use more complex operations.

the processing pipeline is a generic node graph (DAG) which supports multiple
inputs and multiple outputs. all processing is done in glsl shaders/vulkan. the
gui profits from this scheme as well and can display textures while they are
still on GPU.

## features

* [very fast GPU only processing](src/qvk/readme.md)
* [general DAG of processing operations](src/pipe/readme.md), featuring multiple inputs and outputs and
  feedback connectors for animation/iteration
* [full window colour management](doc/howto/colour-display/readme.md)
* [noise profiling](doc/howto/noise-profiling/readme.md)
* [minimally invasive folder-based image database](src/db/readme.md)
* [command line utility](src/cli/readme.md)
* [real time magic lantern raw video (mlv) processing](src/pipe/modules/i-mlv/readme.md)
* [GPU decoder for MotionCam raw video](src/pipe/modules/i-mcraw/readme.md)
* [10-bit display output](src/pipe/modules/test10b/readme.md)
* [automatic parameter optimisation](src/fit/readme.md), for instance to fit vignetting
* [heavy handed processing](src/pipe/modules/kpn-t/readme.md) at almost realistic speeds

## documentation

* [image operation modules documentation](src/pipe/modules/readme.md)
* [how to do selected things](doc/howto/howto.md)

## packages

there are [nightly packages built by the github ci](https://github.com/hanatos/vkdt/releases/tag/nightly).

also there is a [nixos package](https://search.nixos.org/packages?channel=unstable&show=vkdt) `vkdt`
which you can use to try out/run on any linux distro, for instance:
```
  nix-shell -p vkdt
```
`vkdt` comes with a nix flake for an up-to-date package, so you
can checkout this repository and then
```
  nix run .
```
if you're on arch, there is a vkdt-git package in aur:
```
  yay vkdt-git
```


## build instructions

you should then be able to simply run 'make' from the bin/ folder. debug
builds enable the vulkan validation layers, so you need to have them
installed.
```
  cd bin/
  make -j20
```

simply run `make debug` for a debug build. note that the debug
build includes some extra memory overhead and performs expensive checks and can
thus be much slower. `make sanitize` is supported to switch on the address
sanitizer. changes to the compile time configuration as well as the compiler
toolchain can be set in `bin/config.mk`. if you don't have that file yet, you can
copy it from `bin/config.mk.defaults`.

## running

the binaries are put into the `bin/` directory. if you want to run `vkdt` from
anywhere, create a symlink such as `/usr/local/bin/vkdt -> ~/vc/vkdt/bin/vkdt`.
```
  cd bin/
  ./vkdt -d all examples/logo.cfg
```
raw files will be assigned the `bin/default-darkroom.i-raw` processing graph.
if you run the command line interface `vkdt-cli`, it will replace all `display`
nodes by export nodes.
there are also a few example config files in `bin/examples/`. note that you
have to edit the filename in the example cfg to point to a file that actually
exists on your system.

## licence

our code is licenced under the 2-clause bsd licence (if not clearly marked
otherwise in the respective source files, which contain a bit of viral GPLv3,
so handle with care if you're afraid of the GPL). there are parts from other
libraries that are licenced differently. in particular:

rawspeed:     LGPLv2
ffmpeg:       LGPLv2
nuklear:      public domain

and we may link to some others, too.

## dependencies
* vulkan, glslangValidator (libvulkan-dev, glslang-tools, or use the sdk)
* glfw (libglfw3-dev and libglfw3, use >=3.4 if you can for best wayland support)
* for raw input support:
  * either rawspeed (depends on pugixml, stdc++, zlib, jpeg, libomp, build-depends on cmake, libomp-dev, optionally libexiv2)
  * or rawler (depends on rust toolchain which will manage their own dependencies)
* libjpeg
* video io: libavformat libavcodec (minimum version 6)
* sound: libasound2
* build: make, pkg-config, clang, sed, xxd

the full list of packages used to build the nightly appimages can be found in the [workflow yaml file](.github/workflows/nightly.yml).

optional (configure in `bin/config.mk`):

* exiv2 (libexiv2-dev) for raw metadata loading to assign noise profiles, only needed for rawspeed builds
* asound (libasound2) for audio support in video and quake
* ffmpeg (libavformat-dev libavcodec-dev) for the video io modules `i-vid` and `o-vid`

you can also build without rawspeed or rawler if that is useful for you.


## faq
* **which platforms are supported**
`vkdt` has native support for nix os, arch linux via aur, and builds and runs
on macintosh (intel and apple silicon, use brew to build and the lunarg vulkan
sdk to run). there is an unfinished android port (see pr on github).

* **does it work with wayland?**  
`vkdt` officially runs natively on wayland and *better* than on x11. use
glfw>=3.4 for this. details vary a lot with compositor, hyprland is excellent.
caveat and pain point is colour management, the builtin mechanism in `vkdt`
works, but only on a single screen. creating the display profile depends on
argyll/displaycal of course and still requires x11.

* **can i run my super long running kernel without timeout?**  
if you're using your only gpu in the system, you'll need to run without xorg,
straight from a tty console. this means you'll only be able to use the
command line interface `vkdt-cli`. we force a timeout, too, but it's
something like 16 minutes. let us know if you run into this..

* **can i build without display server?**  
there is a `cli` target, i.e. you can try to run `make cli` to only generate
the command line interface tools that do not depend on xorg or wayland or glfw.

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
to work with large graphs or 50MP+ images you want at least 8GB video ram.

* **can i speed up rendering on my 2012 on-board GPU?**  
you can set the level of detail (LOD) parameter in your
`~/.config/vkdt/config.rc` file: `intgui/lod:1` (1 is the maximum resolution,
render all the pixels). set it to `2` to only render exactly at the resolution
of your screen (will slow down when you zoom in), or to `3` and more to brute
force downsample.

* **can i limit the frame rate to save power?**  
there is the `frame_limiter` option in `~/.config/vkdt/config.rc` for this.
set `intgui/frame_limiter:30` to have at most one redraw every `30` milliseconds.
leave it at `0` to redraw as quickly as possible.

* **where can i ask for support?**  
try `#vkdt` on `oftc.net` or ask on [pixls.us](https://discuss.pixls.us/c/software/vkdt).
