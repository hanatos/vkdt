# i-obj: import wavefront object triangle meshes

this module reads an `.obj` file and provides it as triangle mesh ssbo
to be passed on to further geometry node processing or [a `bvh` module](../bvh/readme.md).

the `texids.txt` text file referred to by the `texids` parameter holds a list of object names
as in the `o objname` lines in the `.obj` file, and their associated texture ids. the format is
```
objname_base:2
objname_emit:3
objname_norm:4
..
```
note that texture id zero is reserved for the codebook texture.
normals are expected to live in tangent space with dpdu and dpdv rotated according to the
texture coordinates.

## connectors

* `output` an ssbo containing the triangle mesh. connect to geometry nodes or a bvh builder module.

## parameters

* `filename` the input obj filename
* `startid` the id to use for the first frame in case the filenames are animated
* `texids` the filename of a text file associating obj object names and texture ids
