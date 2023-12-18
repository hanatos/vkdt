# cnngenin: generate training data as input for nn optimisation

this takes a sequence of input and reference image pairs and prepares
them for neural network training such as `kpn-t` over the course of an
animation.

every frame will present a different tile with different rotation and
exposure, to generate more variation that present in the dataset.

this module is meant to be connected to a chain of input that stems from
sequence data (use the `%04d` pattern in the filename input of `i-jpg`, say).

## connectors

* `imgi` the input image to be processed
* `refi` the reference image to be compared to
* `imgo` a random tile of the input image
* `refo` the corresponding tile of the reference image

## parameters

* `off` offset xy
* `flip` flip the image in x or y
* `ev` exposure value
* `generate` switch randomisation of input on or off
