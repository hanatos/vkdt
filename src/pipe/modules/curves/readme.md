# curves: rgb or ych combination curve controls

this module adds a widget to control colour by curves.
select which curve to edit in the combo box: this can either simply be rgb per channel, luminance,
or all 3x3 combinations of luminance (Y) vs chroma (c) vs hue (h).
click into the spline curve widget (the diagonal curves) to add a new control point, right click to remove the
vertex under the mouse. the curves are cubic splines, so you can control the
slopes at the beginning and end of the spline (via the sliders for `ddr0` and
`ddrn`, for instance).

the flat curves for the off-diagonal combinations of for instance Y/c or c/h start as a straight
horizontal line and have a fixed number of six vertices.

unlike the `filmcurv` module, this module attempts no colour correction of
any kind on top of the changes directly reflected by the curves.

## parameters

* `channel` show r,g, or b curve for editing in the dspy widget
* `ych 3x3` choose which of the 3x3 combinations of luminance vs chroma vs hue to edit. these are all active at the same time.
* `mode` choose to which channel to apply curves to
* `ddr0` the slope of the curve before the leftmost control vertex
* `ddg0` the slope of the curve before the leftmost control vertex
* `ddb0` the slope of the curve before the leftmost control vertex
* `ddrn` the slope of the curve after the rightmost control vertex
* `ddgn` the slope of the curve after the rightmost control vertex
* `ddbn` the slope of the curve after the rightmost control vertex
* `edit` edit linear values or in some gamma corrected space that gives more control over blacks. this only affects the ui.

## connectors

* `input` attach either scene referred linear or display referred here. the curves operate in [0,1] but extend past the last control point with constant slopes
* `output` the processed image, with curves applied
* `dspy` the interactive visualisation of the three curves

