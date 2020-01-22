# colour management

the whole window is colour managed, i.e. everything
is rendered in linear rec2020 and alpha blended there.
conversion to the display profile is done in
``imgui/examples/imgui_impl_vulkan.cpp`` in the fragment
shader code.

## installation
you need to profile your display, apply the TRC with dispwin,
and supply the matrix to ``vkdt``. the latter is currently
done by running the script ``bin/read-icc.py`` which will
generate a ``display.profile`` file containing the extracted
gamma and colour matrix. if this is found in the bin directory
during run time, ``vkdt`` will pick it up and load it to the gpu.

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
