# processing graph

we want to support arbitrary numbers of input and output channels. this means
under the hood have a full blown node graph, and modules need to describe
their i/o and buffer layouts in the most generic way. for vulkan, we turn
this into a command buffer with dependencies.

we can have multiple sources (many raw images, 3d lut, ..) and many sinks
(output for display, many tiles of output, histogram, colour picker, ..).

this is still called "pipeline" because we'll need to push it into a somewhat
linear pipeline for execution on the gpu (via topological sort of the DAG).

## memory

we perform one big vkAllocateMemory and bind our buffers to it. each
input/output image can be one buffer, all temp/staging stuff we have as one
buffer, too.  we'll push offsets through to the shader kernel if more chunks
are required. unfortunately we can only have 128B/256B push constants, so these
are used sparingly for iteration counters. module parameters are stored in
uniform memory.

graph.h transforms the DAG to a schedule for vulkan. it considers dependencies
and memory allocation (and would initiate tiling if needed).


## layers

the pipeline has a few different layers:

* module layer, needs to read tokens for nodes and connections from
ascii/binary file. this connects modules and sources/sinks.

* self-configuring node layer: nodes inside a module aren't visible to
the outside. these connections and node counts may depend on the regions of
interest currently being processed. also, if the roi is not full, it will
come with a context buffer (preview pipe). such roi+context may be packed into
one connector that appears in the module layer. the module connects
all intermediate buffers from the context layer to all necessary nodes.
this layer could be handled by structs + pointers.
might interface with this layer for debugging (reconnect intermediates to
display sinks)

this complete DAG of all atomic nodes directly translates into vulkan/glsl with
one compute shader per node.


# pipe configuration io

we setup the processing pipeline/node graph from
scratch from a simple description format:

```
project.cfg
```

# pipeline configuration/parametrs

a storage backend would then either serialise this into human readable
ascii form:

```
exposure:ev:2.0
exposure:black:-0.01
colorin:matrix:1:0:0:0:1:0:0:0:1
..
```

or if required for efficiency reasons could store it in binary as the uint64
and values in floating point (some special byte to mark end of line)

# meta information

the parameter interface supports strings. these will be transferred as uniform
memory as they are by default, but there is a `commit_params` callback for
custom translation of strings to floats (say lensfun name to polynomial
coefficients).
