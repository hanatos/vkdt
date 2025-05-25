# bvh: sink for ray traced geometry

a bvh (bounding volume hierarchy) is in acceleration structure for ray tracing.
this sink here will append the triangle data that is handed in to a global ray tracing bvh
by constructing a bottom level acceleration structure which will be combined globally with
all other `bvh` modules you might have on a graph. input might come from [an obj file via the `i-obj` module](../i-obj/readme.md).

note that it is not necessary to include an edge from here to any ray tracing nodes.

currently marking a bvh as static does not avoid re-running potential compute shaders on the input.

## connectors

* `input` wire triangulated geometry here, say from an i-obj module

## parameters

* `animate` determine whether this geometry is static or dynamic. static will only be built once and reused.
