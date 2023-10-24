# mv2rot: estimate frame rotation and movement from motion vectors

this module estimates a rotation and translation from a set of motion vectors.
it uses a ransac algorithm.

## connectors

* `input` the input image to align
* `mv` motion vectors like from the align module
* `output` the aligned image
