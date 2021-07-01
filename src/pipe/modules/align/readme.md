# image alignment

this module is an implementation of [Johannes Hanika, Lorenzo Tessari, and
Carsten Dachsbacher, Fast temporal reprojection without motion vectors, Journal
of Computer Graphics Techniques, 2021](https://jo.dreggn.org/home/2021_motion.pdf).
it can be useful to align consecutive frames in real-time renders, as well as
to stack hand-held short exposure photography for low-light or hdr.

## connectors

* `alignsrc`  a feature map of the to-be-aligned image
* `aligndst`  a feature map to be aligned to (these will stay static)
* `input`  the input image pixels which will be warped
* `output` the warped output image
* `mask`   the error mask (output)

plus debug channels

## parameters

the parameters are as described in the paper.

* `merge_k` increase this to give the error mask more weight (i.e. reduce potential misalignment artifacts)
* `merge_n` increase this to instruct the error mask that small errors are likely noise and can be ignored
* `blur0`   these blur parameters control the size of the feature detection context (patch size) on different scales. 0 is the finest and 3 the coarsest.
* `blur1`
* `blur2`
* `blur3`


## technical

please see the paper for details. in short, pixel correspondence detection is
based on a hierarchical application of non-local means.
