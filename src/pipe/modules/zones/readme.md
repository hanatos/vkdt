# zone system

this is inspired by ansel adams zone system, a way to think
about a tone curve, i.e. modifying brightness of the output
based on logarithmically spaced zones of input brightness.

it works similar to the tone equaliser aurielien implemented
for darktable: the input image is partitioned into zones
based on exposure values. these partitions are blurred to
give us smooth blending. this is done by a guided filter which
takes the original image as guide for the edges.
