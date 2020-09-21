# deconvolution

this is a straightforward implementation of richardson lucy deconvolution.
it's inspired by rawtherapee's "capture sharpening" and can help reduce
some blur caused by optical low pass filters, blurry lenses, or soft
demosaicing.

it should be run on linear data just after demosaicing.

## parameters

* `sigma` the parameter of the gaussian blur assumed to have deteriorated the image
* `iter` number of iterations to run the algorithm for. runtime scales pretty much linearly in this number.
