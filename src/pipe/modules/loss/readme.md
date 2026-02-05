# loss: compute a loss value for fitting purposes

this modules serves to compute a loss function to guide non-linear optimisation
of module parameters. to this end, it reads an input buffer with some processing applied
to it. this is then compared against the reference image.
after computing the loss, it will be stored (cpu side) in a parameter `loss`.

## connectors

* `input` the processed image
* `orig` the reference image
* `dspy` a display channel to output the masked pixels that are considered during loss computation
* `loss` pass the loss on as a gpu buffer for optimisers or csv output modules
* `vis` a buffer visualising the per-pixel loss

## parameters

* `bound` : only pixels within these bounds are considered in the loss. pixel brightness has to be `> bound.x`, `< bound.y` and saturation (i.e. max channel minus min channel) has to be `< bound.z`.
* `loss` : the computed loss will be stored here if download sink was enabled during graph processing.

