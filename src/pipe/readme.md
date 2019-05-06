# pipeline design

we want to support arbitrary numbers of input and output channels. this means
under the hood we'll need a full blown node graph, and modules need to describe
their i/o and buffer layouts in the most generic way. for vulkan, we'll turn
this into a command buffer with dependencies.

this is still called "pipeline" because we'll need to push it into a somewhat
linear pipeline for execution on the gpu (via topological sort of the DAG).

TODO: see graph.h

memory:
we want one big allocation vkAllocateMemory and bind our buffers to it. each
input/output image can be one buffer, all temp stuff we want as one buffer,
too.  we'll push offsets through to the shader kernel if more chunks are
required. unfortunately we can only have 128B/256B push_constants, which
may mean depending on the limit in VkPhysicalDeviceLimits, we'll need to
resort to uniform buffers instead.

graph.h transforms the DAG to a schedule for vulkan. it considers dependencies
and memory allocation and initiates tiling if needed.

we can have multiple sources (many raw images, 3d lut, ..) and many sinks
(output for display, many tiles of output, histogram, colour picker, ..).


# pipe configuration io

for each image, we want to setup the processing pipeline/node graph from
scratch from a simple description format:

TODO: see node.h

TODO: also need simple description for input/output/temp requirements
for each iop so we can auto-generate the vulkan pipelines


# image operation (iop) interface

we'd like to be able to set and get parameters in both an efficient and human
readable manner.

modules and params should have a name as string (8-chars or 8-byte int)
to facilitate both readable interaction as well as efficient comparison:

```
typedef union
{
  char str[8];
  uint64_t id;
}
token_t;
```

the initialisation
```
token_t tkn = {.id = 0xfffful };
```
is simple, the other way around requires us to pad up the string to exactly 8
chars, so we'll need an api function that takes care of this.  hopefully this
code path should never be triggered in performance critical steps (either hard
coded/compile time or when reading human readable input which should be rare).

we can then get and set parameters with a generic interface:

```
set(token_t module, token_t param, value)
```

open issues:
* how to assign module versions here? always pass in through set?
* type system for values? we can only pass int and float to the compute shader,
  so is always float + a size enough?

maybe:
```
setn(token_t module, token_t param, int num, *value)
setv(token_t module, token_t param, int version, value)
setnv(token_t module, token_t param, int num, int version, *value)
```

# pipeline configuration/parametrs

a storage backend would then either serialise this into human readable
ascii form:

```
exposure ev 2.0
exposure black -0.01
colorin matrix 1 0 0 0 1 0 0 0 1
..
```

or if required for efficiency reasons could store it in binary as the uint64
and values in floating point (some special byte to mark end of line)

# meta information

this would be input image file name and similar for input nodes as well as lens
profiles/exif data, etc. in general strings that are required to setup the node
graph but will not be uploaded to the gpu. most of this can probably be derived
from the image and metadata automatically, some we would probably want to
store in the above format as strings.
