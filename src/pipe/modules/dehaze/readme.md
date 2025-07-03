# dehaze: compute depth map from haze

computes a depth map by estimating the haze in the picture.
follows **Single image haze removal using dark channel prior**, He, Sun, Tang, CVPR 2009.

## connectors

* `input` linear scene referred data
* `output` a depth map estimated from haze in the image

## parameters

TODO: some omega determining how much thick haze is left in the image
TODO: some deep haze colour, picked from the image?
