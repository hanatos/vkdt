# ychcurve: luminance, chroma, hue 3x3 curve controls

assume rec2020, display referred might work too

## parameters


## connectors

* `input` attach either scene referred linear or display referred here. the curves operate in [0,1] but extend past the last control point with constant slopes
* `output` the processed image, with per-channel curves applied
* `dspy` the interactive visualisation of the 3x3 curves

