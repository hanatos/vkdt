# curves: rgb curve controls

this module adds a simple widget to control rgb curves.
select which curve to edit in the combo box.
click into the curve widget to add a new control point, right click to remove the
vertex under the mouse. the curves are cubic splines, so you can control the
slopes at the beginning and end of the spline (via the sliders for `ddr0` and
`ddrn`, for instance).

unlike the `filmcurv` module, this module attempts no colour correction of
any kind on top of the changes directly reflected by the curves.

## parameters

* `channel` show r,g, or b curve for editing in the dspy widget
* `ddr0` the slope of the curve before the leftmost control vertex
* `ddg0` the slope of the curve before the leftmost control vertex
* `ddb0` the slope of the curve before the leftmost control vertex
* `ddrn` the slope of the curve after the rightmost control vertex
* `ddgn` the slope of the curve after the rightmost control vertex
* `ddbn` the slope of the curve after the rightmost control vertex

## connectors

* `input` attach either scene referred linear or display referred here. the curves operate in [0,1] but extend past the last control point with constant slopes
* `output` the processed image, with per-channel curves applied
* `dspy` the interactive visualisation of the three curves

