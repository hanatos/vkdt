# filmcurv: compress scene referred linear for display

this module applies a film-style curve to the image.
the curve is parametric and uses the [weibull distribution](https://en.wikipedia.org/wiki/Weibull_distribution).

## parameters

* `light` change the overall brightness of the output. this is the reciprocal lambda parameter of the weibull distribution
* `contrast` contrast of the output. this is the k parameter of the weibull distribution
* `bias` offset to add to black pixels. use to lift for low contrast renders and to correct elevated raw black level
* `colour` how to reconstruct colour after applying the curve to the brightness channel. darktable ucs has in general better colours, but hsl is more robust for edge cases near black. munsell is data driven and navigates brightess by munsell coordinates. agx aims to replicate how film and human eyes perceive of light.

## connectors

* `input`
* `output`
* `dspy` a plot of the curve that corresponds to the current parameters. as dspy output it will be connected
   to a temporary display when you expand the module

## technical

colour reconstruction is currently done by applying the curve to 
all three rgb channels simultaneously, followed by the saturation
preservation step as found in the dng spec and rawtherapee.
