# accumulate images

this module implements frame accumulation. the output will be
```
out = input/(N+1.0) + N/(N+1.0) * back
```
where `N` is 0 if frame is the start parameter.
you can use this together with a feedback connector from `output` to `back` to
implement a framebuffer for Monte Carlo integration.

## connectors

* `input` one input buffer
* `back` the back buffer
* `output` the accumulated output

## parameters

* `start` the first frame to start accumulating


