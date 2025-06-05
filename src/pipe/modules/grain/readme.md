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

[![example grain](grain.jpg)](grain.jpg)

## connectors

* `input` display referred image normalised between zero and one
* `output` the image with film grain applied

## parameters

* `size` the size of the grain, with respect to pixel size (this is not scale invariant)
* `strength` the magnitude of the effect, how much will be added
* `decay` how fast do lower frequency components of the noise decay. set this to 0 for only high frequency noise.
* `octaves` how many low frequencies to consider. set this to 1 for only high frequency noise.

