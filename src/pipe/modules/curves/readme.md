# curves: rgb or ych combination curve controls

this module adds a widget to control colour by curves. select which curve to
edit in the combo box: this can either simply be rgb per channel, luminance, or
all 3x3 combinations of luminance (Y) vs chroma (C) vs hue (h). click into the
spline curve widget (the diagonal curves) to add a new control point, right
click to remove the vertex under the mouse. the curves are cubic splines, so
you can control the slopes at the beginning and end of the spline (via the
sliders for `ddr0` and `ddrn`, for instance).

the flat curves for the off-diagonal combinations of for instance Y/C or C/h
start as a straight horizontal line and have a fixed number of six vertices.

unlike the `filmcurv` module, this module attempts no colour correction of
any kind on top of the changes directly reflected by the curves.
it also contains a simple Y/Y luminance curve:

![YY curve](curve-luma.jpg)

the diagonal entries of the YCh curves are by default an identity transform,
i.e. a diagonal line. the off-diagonals are applied on top of these diagonal
curves, and are by default a horizontal line that can be adjusted up or down:

![C/h curve](curve_ch.jpg)

the background shows a coloured histogram of the values in the input image
corresponding to the x-axis (luminance Y, chroma C or hue h). the plot
always shows all three curves of the same row, i.e. depending on the
same variable.

this tool is similar in spirit to [rawtherapee's lab adjustments](https://rawpedia.rawtherapee.com/Lab_Adjustments)
but works in a linear opponent space (YCbCr, the radial version of it).


## parameters

* `channel` show r,g, or b curve for editing in the dspy widget
* `ych 3x3` choose which of the 3x3 combinations of luminance (Y) vs chroma (C) vs hue (h) to edit. these are all active at the same time.
* `mode` choose to which channel to apply curves to
* `ddr0` the slope of the curve before the leftmost control vertex
* `ddg0` the slope of the curve before the leftmost control vertex
* `ddb0` the slope of the curve before the leftmost control vertex
* `ddrn` the slope of the curve after the rightmost control vertex
* `ddgn` the slope of the curve after the rightmost control vertex
* `ddbn` the slope of the curve after the rightmost control vertex
* `edit` edit luminance as linear values or in some gamma corrected space that gives more control over blacks. this only affects the ui and only rgb curves and luminance
* `radius` size of the guided filter chroma/hue blur to avoid colour noise
* `edges` edge parameter of the guided filter chroma/hue blur to avoid colour noise

## connectors

* `input` attach either scene referred linear or display referred here. the curves operate in [0,1] but extend past the last control point with constant slopes
* `output` the processed image, with curves applied
* `dspy` the interactive visualisation of the three curves

