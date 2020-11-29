# film curve

this module applies a film-style curve to the image.
the curve is parametric and run by two parts: a logarithmic shaper followed
by a contrast-s curve.

## parameters

* `x0-3` and `y0-3` define the four control points of the contrast-s curve.
  the default is a straight line. the curve will be a cubic hermite spline.
* `black` this controls the logarithmic shaper. intuitively, it is the ev number
  where you want to locate black. the more you go negative, the further away
  you are pushing the black point. this means, relative to that, all pixel values
  will become brighter and contrast is compressed more. use this to flatten high
  dynamic range images. a large value of black (say 1.0) will result in a more
  contrasty image.

## connectors

* `input`
* `output`
* `curve` a plot of the curve that corresponds to the current parameters.
  connect this to say `display:hist:input` to see the curve update interactively.

## technical

probably worth looking into a graphical widget for the curve.

colour reconstruction is currently done by applying the curve to 
all three rgb channels simultaneously. it should probably use the
Mantiuk 2009 method such as *local contrast* does, too.
