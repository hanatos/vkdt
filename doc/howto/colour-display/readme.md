# display colour management

the following writeup is about display colour management: how to install icc
profiles for your screens.
for camera characterisation, i.e. input colour
management, please see
[the `colour` module](../../../src/pipe/modules/colour/readme.md)
as well as
[the input device transform creation tools](../../../src/tools/clut/readme.md).


in vkdt, the whole window is colour managed, i.e. everything is rendered in linear
rec2020 and alpha blended there. conversion to the display profile is done in
fragment shader code. this is either done on the vkdt side (xorg) or in the compositor
(newer wayland).

## profiling

this is required in any case: obtain a profile characterising the colour of your monitor.
in general, you'll need to profile your display (follow the instructions in
[the excellent dcamprof article mirrored by
rawtherapee](https://rawtherapee.com/mirror/dcamprof/argyll-display.html)).

argyll/displaycal provide means to do the profiling. with some caveats this
might work on wayland too. otherwise there is a [display
profiler](https://invent.kde.org/zamundaaa/displayprofiler.git) for wayland/kde
which might work with your colorimeter.


## installation on xorg

handing the display matrix to `vkdt` is done with a text file, created by
```bash
$ vkdt read-icc <your-monitor.icc>`
```
the file contains three gamma values and a 3x3 matrix from rec2020 to display
colour space. when `vkdt` is started from a console, you'll notice log output
like
```
[gui] monitor [0] HDMI-A-1 at 0 0
[gui] monitor [1] DP-1 at 0 0
```
which identify the names of your monitors. in this case, `vkdt` will search
for `display.HDMI-A-1` and `display.DP-1` in the system-wide `vkdt` binary
directory as well as in your home directory `~/.config/vkdt/`.

apply the TRC using `dispwin`, and supply the matrix to `vkdt` (see above).

`vkdt` only supports matrix profiles for the output device transform. assuming
monitors with linear response to the input on their primaries (after applying
TRC), this is mathematically sufficient to reproduce the right colour for a
human observer. the reason is that the colour is already encoded as tristimulus
values in a colour space that has the same metameric behaviour as the cie
observer (rec2020 in our case here). this does not hold true for [input device
transforms](../../../src/tools/clut/readme.md), which in general require more complex
transforms.
as an additional simplification, currently only gamma is supported, no generic
shaper curve.



## wayland
newer compositors allow to run in rec2020 mode, for instance if you run
hyprland, you can put a line into your `hyprland.conf` like
```
monitor=DP-1,3840x2160@160.0,0x0,1.5,bitdepth,10,cm,wide
```
note the `cm,wide`, which sets the compositor to wide gamut BT2020.


### high dynamic range
on wayland, colour management is tightly coupled to high dynamic range (hdr) mode.
in hyprland, you can enable hdr for the display via `hyprland.conf`:
```
monitor=DP-1,3840x2160@160.0,0x0,2.0,bitdepth,10,cm,hdr
```
new sway will also allow us to open a window in hdr10 mode, and thus speak
to the compositor in BT2020. kde supports such mode of operation, too.

`vkdt` supports rendering to hdr framebuffers, using HDR10 ST2084, i.e. BT2020
primaries and a perceptual quantiser (PQ) tone response curve. these are preferred
over the sdr variants.
look for output like
```
[qvk] using A2R10G10B10_UNORM_PACK32 and colour space HDR10 ST2084
```
indicating that a 10-bits per channel hdr framebuffer is used, or:
```
[qvk] using A2R10G10B10_UNORM_PACK32 and colour space sRGB nonlinear
```
in which case a standard 10-bit sdr buffer is used.
while on macintosh things seem to "just work", windows generally makes a mess of the image.
thus, there is a config option to switch off hdr framebuffers even though they are
available. this defaults to "hdr disabled" and thus anyone on all platforms has to
flip the switch in `~/.config/vkdt/config.rc`:
```
intgui/allowhdr:1
```
to actually use HDR10/PQ framebuffers.

note that sway offers this even if the monitor is not set to hdr mode, so we
use this for colour management. apply your icc profile in sway by something like
```
sway output DP-1 color_profile icc my-profile.icc
```

note that if a PQ framebuffer is used, the `display.DP-1` files go unused/ignored.

at first you might think vkdt and the pictures within look a bit dimmer than without
hdr enabled. this is because the display transform usually tonemaps everything into
limited dynamic range. to see the highlights, disable the `filmcurv` module and push
exposure way up to observe the brighter highlights.
you can adjust the luminance level of the PQ curve via the following setting
in `~/.config/vkdt/config.rc`:
```
fltgui/hdr_avg_nits:64
```

by default vkdt uses maximum luminance of 1000 nits and minimum 0 nits.



### hdr metadata

(safe to ignore this section)
some compositors support the vulkan extension for hdr metadata, `VK_EXT_hdr_metadata`.
on hyprland, you'd still need to install the [vulkan WSI
layer](https://github.com/Zamundaaa/VK_hdr_layer.git). kde/plasma ships it
built-in. then, if you run
```
$ ENABLE_HDR_WSI=1 vkdt -d qvk
```
the extension will be loaded and the swapchain images will be annotated with extra
information about primaries and min/max nits.
i have no evidence that this data is used in any way by anyone.


## details for more esoteric use cases

### gui
all gui colours need to be given in rec2020 tristimulus values (in particular
in `style.txt`).

### thumbnails
bc1 thumbnails are stored in gamma/sRGB. they are loaded as
`VK_FORMAT_BC1_RGB_SRGB_BLOCK` such that they are linear
when used in the gui.

### pipe
the pixel pipeline/graph works in linear rec2020 encoding.
the output in display nodes (f16) is read as linear, and the
imgui fragment shader multiplies the icc matrix and applies
the gamma shaper.

if the display supports 10 bits, the vulkan framebuffers are
inited as 10bit format, and the final result in the swapchain
will be rendered at that precision (all our internal buffers
have higher precision).

### cli
the command line interface explicitly inserts `colenc` nodes before
the `o-*` nodes if the output format requires gamma encoding,
such as jpeg.
