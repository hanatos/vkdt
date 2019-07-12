# graphical user interface (gui)

this initialises a vulkan context on screen and renders image thumbnails as well
as ui elements on the GPU (immediate mode gui).

because imgui is a c++ interface, this module has a c++ part which takes care
of the gui rendering for the image processing modules. the behaviour is steered
by .ui annotation files, the modules themselves have no gui code.
we make sure nobody else will ever call gui functions by writing the rest in c,
forcing core and gui code to be separate.

# lighttable mode

list of images, cached in several buffers.
again, we'll want only one vkAllocateMemory, and maybe one
buffer per mipmap level?
the buffers per mip can be same size and contain all the thumbnails
currently cached.
