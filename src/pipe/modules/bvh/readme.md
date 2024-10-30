# bvh: sink for ray traced geometry

a bvh (bounding volume hierarchy) is in acceleration structure for ray tracing.
this sink here will append the triangle data that is handed in to a global ray tracing bvh
by constructing a bottom level acceleration structure which will be combined globally with
all other `bvh` modules you might have on a graph.

note that it is not necessary to include an edge from here to any ray tracing nodes.

TODO:
* static geometry: flag nodes somehow so BLAS build and dependent compute shaders will be skipped
* refactor raytrace.c to not allocate staging memory but use ssbo handed in here
* `s_conn_double_buffered` as we do for `display` nodes.
* raytrace.c to use 17-f16-strided vertex arrays with R16G16B16 non-indexed triangles + extra data in the gaps
* producing modules: load obj, spawn particles, deform, animate, extrude with particle sim

## connectors

* `input` wire triangulated geometry here
