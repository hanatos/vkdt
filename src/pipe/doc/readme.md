# pipeline setup

the pipeline is organised in layers. the first, user visible, layer is a graph
on modules. these have config params and can be arranged freely, and quickly
read from human readable or binary config files. the next layer is nodes
within each module. every module has at least one. a node will map directly
to a shader kernel, and its inputs and outputs will be managed by our memory
manager.

## modules

this is an example graph connecting a few modules, with multiple sinks and
multiple sources:

![](images/modules.svg)


## nodes

this is one example for the local laplacian module which has particularly many
inner nodes:

![](images/nodes.svg)

TODO: image is incomplete. connections and number of reduce nodes depend on
configuration (contrast curve frequency might be hard coded, but resolution
determines number of levels in pyramids).

external connections are indicated as thick lines. these obviously take the
input to "pad" and the output from the last "assemble" stage. maybe not so
obviously it also has a connection to do tone manipulations for the coarsest
level.  this is useful for hdr images for instance, in this case.

note that in general it is more complicated than this: every external connection
can potentially come with a region of interest and a context buffer. that means
there will be two vulkan buffers associated with the connection.
TODO: simple gaussian blur with context buffer
