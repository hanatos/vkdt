# accumulate images

this module implements frame blending. the `output` will be (1-`opacity`) x
`input` + `opacity` x `back`.
you can use this together with feedback connectors to implement
exponentially weighted averages or framebuffers for monte carlo integration
with a known sample count.

## connectors

* `input` one input buffer
* `back` the other input buffer
* `output` the blended output

## parameters

* `opacity` the blend weight between the two input buffers


