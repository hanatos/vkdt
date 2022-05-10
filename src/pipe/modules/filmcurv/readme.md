# film curve

this module applies a film-style curve to the image.
the curve is parametric and uses the [weibull distribution](https://en.wikipedia.org/wiki/Weibull_distribution).

## parameters

* `light` change the overall brightness of the output. this is the reciprocal lambda parameter of the weibull distribution
* `contrast` contrast of the output. this is the k parameter of the weibull distribution

## connectors

* `input`
* `output`
* `dspy` a plot of the curve that corresponds to the current parameters. as dspy output it will be connected
   to a temporary display when you expand the module

## technical

colour reconstruction is currently done by applying the curve to 
all three rgb channels simultaneously, followed by the saturation
preservation step as found in the dng spec and rawtherapee.
