# display colour management

the following writeup is about display colour management:
how it works, how to install icc profiles for your screens
and a few related details. for camera characterisation, i.e.
input colour management, please see
[the `colour` module](../src/pipe/modules/colour/readme.md)
as well as
[the input device transform creation tools](../src/tools/clut/readme.md).


the whole window is colour managed, i.e. everything
is rendered in linear rec2020 and alpha blended there.
conversion to the display profile is done in
``imgui/examples/imgui_impl_vulkan.cpp`` in the fragment
shader code.

## installation
you need to profile your display, apply the TRC with dispwin,
and supply the matrix to `vkdt`. the latter is currently
done by running the script `bin/read-icc.py` which will
generate a `display.profile` file containing the extracted
gamma and colour matrix.

such a file, if found in the bin directory
during run time, `vkdt` will pick it up and load it to the gpu.
to support dual monitor setups, `vkdt` will look for names such as
`bin/display.HDMI-0` for instance. to see the names of your monitors,
run `vkdt` once with `-d gui` (the default) to list available
monitors. also the loading will complain with the precise file
name it is looking for if it's not found.

currently only single or dual monitors (left/right of another)
are supported.

## gui
all gui colours need to be given in rec2020 tristimulus values.
``gui/render.cc`` has a function to convert user/theme supplied
srgb values to linear rec2020.

## thumbnails
bc1 thumbnails are stored in gamma/sRGB. they are loaded as
``VK_FORMAT_BC1_RGB_SRGB_BLOCK`` such that they are linear
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
the command line interface explicitly inserts ``f2srgb`` nodes before
the ``o-*`` nodes if the output format requires gamma encoding,
such as jpeg.
