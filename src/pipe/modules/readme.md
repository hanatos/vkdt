# iop API

we want these to be as independent of anything as we can. the idea would be to
arrive at a completely reusable pipeline with modules that can easily be linked
into the core as well as reused by other projects if they decide to do so. one
of our immediate use cases is gui frontend and cli which both want to use the
modular pipeline.

thus, we want to limit the calls from here back into the core, ideally avoiding
to link back into a core dso.

one idea, if needed, is that we'll provide an explicit subset of helper functions
as function pointers in a struct that is available to the module.

since modules do their core work in compute shaders, most simple technical helpers
will likely be short functions in glsl headers to be included and inlined.


# note on size negotiations:

preliminaries, to be defined in initialisation phase:
* need to look at raw/input file to get input pixel dimensions
* need to know output roi
* walk over source and sink nodes only

for each output (if many): pull data through graph:
* given output roi, compute input roi + size of
  required scratch pad memory
* topological sort, do depth first graph expansion?
* run through whole graph, keep track of mem requirements:
  * scratch pad mem
  * intermediate inputs at bifurcations
* iterate the above if mem exceeded: split output roi in two, keep
  track of other roi that need to be processed in the end

given all roi and max mem requirements met:
* memory management: reuse scratch pad mem and multiple input buffers
* construct command buffer by running through graph again
* modules may push back varying number of kernel calls depending
  on size
* for tiling and small roi we need a preview buffer run to be 
  interleaved with the regular roi processing. (cache these or just
  reprocess for every tile? usually in dr mode it'll be one tile/roi
  only)


modules can have multiple inputs, but only one output to pull on for roi
computations (scratchmem buffers may change sizes as a side effect).

TODO: though for debugging we can assign scratchmem buffers to a sink node.

TODO: maybe the colour picker and histogram are sink nodes, too? do
we then compute these as side effects or do they actively pull?
this would complicate memory management somewhat.
in dt, these are driven by preview pipe buffers only.

we don't want cycles obviously.

```
source_get_input_size()
sink_get_output_size()

compute_roi_in(input token, output roi)
compute_scratchmem(input token, output roi)

source_read_input(void *)
sink_write_output() // mapped void* or GPU buffer for display?
// separate display output?

// may iterate based on size
// also use preview buffer or not based on full roi vs tile
populate_command_buffer(roi in out)
```

TODO: make as much of the above as possible optional

# list of modules
TODO: and reference all the readme.md in the lower levels
