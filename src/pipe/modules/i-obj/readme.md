# i-obj: import wavefront object triangle meshes

this module reads an `.obj` file and provides it as triangle mesh ssbo
to be passed on to further geometry node processing or [a `bvh` module](../bvh/readme.md).

## connectors

* `output` an ssbo containing the triangle mesh. connect to geometry nodes or a bvh builder module.
