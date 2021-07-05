# vignette fitting loss

this modules serves to compute a loss function to guide non-linear optimisation
of vignette parameters. to this end, it reads an input buffer with vignette
correction applied to it. this is then compared against a constant 1.0 buffer
(ideally corrected).
after computing the loss, it will be stored (cpu side) in a parameter `loss`.

## connectors

* `input` the vignetting-corrected input image, which is ideally constant 1.0
* `dspy` a display channel to output the masked pixels that are considered during loss computation

## parameters

* `bound` only pixels within these bounds are considered in the loss. pixel brightness has to be `> bound.x`, `< bound.y` and saturation (i.e. max channel minus min channel) has to be `< bound.z`.
* `loss` the computed loss will be stored here if download sink was enabled during graph processing.
