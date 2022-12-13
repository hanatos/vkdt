# how to draw masks

also see the faq entry how to compile vkdt such that you can
use your pentablet as pressure-sensitive input:

you need a specific version of glfw to support it.
you can for instance clone `https://github.com/hanatos/glfw`,
for instance to `/home/you/vc/glfw`, and then put the
following in your custom `bin/config.mk`:  
```
VKDT_GLFW_CFLAGS=-I/home/you/vc/glfw/include/
VKDT_GLFW_LDFLAGS=/home/you/vc/glfw/build/src/libglfw3.a
VKDT_USE_PENTABLET=1
export VKDT_USE_PENTABLET
```

of course you can also just use your mouse to draw masks.

for a description how to draw and how the strokes are adapted to the image,
see [the `draw` module documentation](../../../src/pipe/modules/draw/readme.md).
to quickly add a `draw` module and the wiring around it to your image graph,
insert the `draw` block (e.g. by pressing `ctrl-b` in darkroom mode and select
the `draw` block, then insert it before a certain module in the graph).

drawn masks are especially useful in conjunction with
[retouching](../../../src/pipe/modules/wavelet/readme.md) and
[inpainting](../../../src/pipe/modules/inpaint/readme.md).
it can also be required to not only change the exposure of a masked region, but
also [grade it differently](../../../src/pipe/modules/grade/readme.md) to match
the surroundings when removing a shadow, for instance.

also see the (simpler) [graduated density](../../../src/pipe/modules/grad/readme.md)
module which can create a simple linear gradient mask without brush strokes.
