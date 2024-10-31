# bvh: sink for ray traced geometry

a bvh (bounding volume hierarchy) is in acceleration structure for ray tracing.
this sink here will append the triangle data that is handed in to a global ray tracing bvh
by constructing a bottom level acceleration structure which will be combined globally with
all other `bvh` modules you might have on a graph.

note that it is not necessary to include an edge from here to any ray tracing nodes.

TODO:
* static geometry: flag nodes somehow so BLAS build and dependent compute shaders will be skipped
* producing modules: load obj, spawn particles, deform, animate, extrude with particle sim
* quake: use bvh nodes
* `request_read_geo` flag to be called `request_build_bvh`
* `geo` type on ssbo to multiply by known height and use f16 type etc (maybe need memory type flags)

## connectors

* `input` wire triangulated geometry here
