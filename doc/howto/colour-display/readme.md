# display colour management

the following writeup is about display colour management: how to install icc
profiles for your screens for camera characterisation, i.e. input colour
management, please see
[the `colour` module](../../../src/pipe/modules/colour/readme.md)
as well as
[the input device transform creation tools](../../../src/tools/clut/readme.md).


the whole window is colour managed, i.e. everything is rendered in linear
rec2020 and alpha blended there. conversion to the display profile is done in
fragment shader code.

## installation
in general, you'll need to profile your display (follow the instructions in
[the excellent dcamprof article mirrored by
rawtherapee](https://rawtherapee.com/mirror/dcamprof/argyll-display.html)).

handing the display matrix to `vkdt` is done with a text file, created by
```bash
# vkdt read-icc <your-monitor.icc>`
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

### xorg
apply the TRC using `dispwin`, and supply the matrix to `vkdt` (see above).

### wayland
newer compositors allow to run in rec2020 mode, for instance if you run
hyprland, you can put a line into your `hyprland.conf` like
```
monitor=DP-1,3840x2160@160.0,0x0,1.5,bitdepth,10,cm,wide
```
note the `cm,wide`, which sets the compositor to wide gamut rec2020. to
let `vkdt` know, put the identity matrix and rec2020 gamma into the display
profile file, in this case `display.DP-1`:
```
0.454707 0.454707 0.454707
1 0 0
0 1 0
0 0 1
```
going forward this kind of wayland colour management is the preferred way
of handling things. kde supports such mode of operation, too.


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

### hdr

`vkdt` supports rendering to hdr framebuffers, using HDR10 ST2084, i.e. BT2020
primaries and a perceptual quantiser (PQ) tone response curve.
on different platforms, there are slightly different settings for this. in kde
and hyprland, you can enable hdr for the display (system settings or hyprland.conf).
then, run
```
ENABLE_HDR_WSI=1 vkdt -d qvk
```
and look for output like
```
[qvk] using A2R10G10B10_UNORM_PACK32 and colour space HDR10 ST2084
```
indicating that a 10-bits per channel hdr framebuffer is used, or:
```
[qvk] using A2R10G10B10_UNORM_PACK32 and colour space sRGB nonlinear
```
in which case a standard 10-bit sdr buffer is used.

while on macintosh things seem to "just work" (you don't need to enable the wsi
layer as for wayland above), windows generally makes a mess of the image.
thus, there is a config option to switch off hdr framebuffers even though they are
available. this defaults to "hdr disabled" and thus anyone on all platforms has to
flip the switch in `~/.config/vkdt/config.rc`:
```
intgui/allowhdr:1
```
to actually use hdr framebuffers.
