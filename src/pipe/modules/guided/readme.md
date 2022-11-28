# guided: guided filter useful to blur masks

this is a generic implementation of a guided filter.
this version takes no shortcuts and uses both an input and a guided
image.

## connectors

* `input` the input image
* `guide` the guide image
* `output` the output image


## parameters

* `radius` the size of the blur, relative to the input resolution
* `epsilon` the edge threshold
