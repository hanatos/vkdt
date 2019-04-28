# graphical user interface (gui)

this initialises a vulkan context on screen and renders image thumbnails as well
as ui elements on the GPU (immediate mode gui).

because imgui is a c++ interface, we could write this module in c++ (making sure
nobody else will ever call gui functions by writing the rest in c).

# lighttable mode

list of images, cached in several buffers.
again, we'll want only one vkAllocateMemory, and maybe one
buffer per mipmap level?
the buffers per mip can be same size and contain all the thumbnails
currently cached.
