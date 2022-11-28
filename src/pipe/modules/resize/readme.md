# resize: resample images

this module resizes input images. it has no parameters because the
size of the buffer is dictated by the graph surrounding it. it can e.g. be set
in the gui in the export expander or from [the command line](../../../cli/readme.md).

it is necessary to include such a module for inputs that do not by themselves support
scaled input, such as [jpg](../i-jpg/readme.md) or [pfm](../i-pfm/readme.md), so
that they will properly scale on output or for thumbnail rendering.

## connectors

* `input`
* `output`
