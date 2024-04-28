# grain: simulate analog film grain

film grain rendering should be applied on display referred
after scaling to the desired output resolution since the
method assumes input values between zero and one and the grain
size is relative to the pixel size.

this uses a primitive simplex noise and lets you
pick the parameters to make it look good.

the standard deviation of the noise is picked based on
a slightly skewed distribution depending on the pixel luminance.
mid-tones will receive more noise.

## connectors

* `input`
* `output`

## parameters

* `size` the size of the grain, with respect to pixel size (this is not scale invariant)
* `strength` the magnitude of the effect, how much will be added

