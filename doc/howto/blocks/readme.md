# how to manage blocks

on the most fine-grained level, vkdt's processing is organised as a directed
graph of nodes that directly correspond to kernels executed on the device. a
module you usually see in the ui may consist of several such nodes.

for some use cases, however, this is still not coarse grained enough. for
instance if you want to draw brush strokes on an image for dodging or burning,
you'll need a `draw` module, a `blend` module, and as a minimum an `exposure`
module to alter the pixels under the mask at all. to adjust the mask to the
edges of the input images, it is useful to include a `guided` filter module
too. now this is already four additional modules and quite laborious to add
manually.

for such cases, there are *blocks*, templates of sub-graphs to be added as a
whole. the example above is realised in the `draw.cfg` block. to apply it,
press `ctrl-b` (default hotkey) in darkroom mode and select the `draw.cfg`
block. it will query you where to insert the block, since blocks are templates:
they will be inserted between two modules which are already on your graph.
additionally, you can specify the common instance name of the modules that will
be newly added (maybe `dodge` is a good instance name in our example).

blocks are located in the `data/blocks/` subdirectory of the installation, and
custom user-specific blocks will also be read from `~/.config/vkdt/blocks/`.

## creating your own block

as an example, start from the `data/blocks/draw.cfg` block. it is a standard
vkdt `.cfg` file but contains a few variables that will be replaced before
the commands are appended to the history of the image:

* `INSTANCE` the common instance name of new modules
* `OUTMOD`   the module name of the module which comes *before* the block
* `OUTINST`  the instance name of this module
* `OUTCONN`  the connector name of the same
* `INMOD`    the module name of the module which comes *after* the block
* `ININST`   the instance name of this module
* `INCONN`   the connector name of the same.
