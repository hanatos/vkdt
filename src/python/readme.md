# python integration

we want pybind11 style python integration. since this has to be a c++ module,
we'll take the opportunity and define a public api as api.hh.

there will be an interactive python console.

the node graph parameter interface makes it trivial to change pipeline
configuration and module parameters from python.
