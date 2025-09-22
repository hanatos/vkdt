# display colour management

the following writeup is about display colour management:
how it works, how to install icc profiles for your screens
and a few related details. for camera characterisation, i.e.
input colour management, please see
[the `colour` module](../../../src/pipe/modules/colour/readme.md)
as well as
[the input device transform creation tools](../../../src/tools/clut/readme.md).


the whole window is colour managed, i.e. everything
is rendered in linear rec2020 and alpha blended there.
conversion to the display profile is done in
``imgui/examples/imgui_impl_vulkan.cpp`` in the fragment
shader code.

## installation
you need to profile your display (follow the instructions in [the excellent
dcamprof article mirrored by rawtherapee](https://rawtherapee.com/mirror/dcamprof/argyll-display.html)),
apply the TRC with dispwin, and supply the matrix to `vkdt`. the latter is
currently done by running `vkdt read-icc` which will generate a
`display.profile` file containing the extracted gamma and colour matrix (the matrix maps from internal rec2020 to display space, so if your monitor runs rec2020 this should be an identity matrix).

such a file, if found in the bin directory
during run time, `vkdt` will pick it up and load it to the gpu.
to support dual monitor setups, `vkdt` will look for names such as
`bin/display.HDMI-0` for instance. to see the names of your monitors,
run `vkdt` once with `-d gui` (the default) to list available
monitors. also the loading will complain with the precise file
name it is looking for if it's not found.

currently only single or dual monitors (left/right of another)
are supported.

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

## gui
all gui colours need to be given in rec2020 tristimulus values.

## thumbnails
bc1 thumbnails are stored in gamma/sRGB. they are loaded as
`VK_FORMAT_BC1_RGB_SRGB_BLOCK` such that they are linear
when used in the gui.

## pipe
the pixel pipeline/graph works in linear rec2020 encoding.
the output in display nodes (f16) is read as linear, and the
imgui fragment shader multiplies the icc matrix and applies
the gamma shaper.

if the display supports 10 bits, the vulkan framebuffers are
inited as 10bit format, and the final result in the swapchain
will be rendered at that precision (all our internal buffers
have higher precision).

## cli
the command line interface explicitly inserts `colenc` nodes before
the `o-*` nodes if the output format requires gamma encoding,
such as jpeg.

## hdr

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
