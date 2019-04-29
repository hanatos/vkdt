# iop ABI

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
