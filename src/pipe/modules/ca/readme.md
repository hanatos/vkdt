# ca: correct chromatic aberrations

this is a straight forward implementation of chromatic aberration
correction: it uses an edge detection pass and fits a polynomial to
the radial offsets of red and blue channels as compared to the green
reference channel. this is then corrected in a second pass.

the module is based on automatic detection, it has no parameters.

## connectors

* `input` the input connector
* `output` the output connector with chromatic aberrations corrected

## parameters

## examples

zoom-in to the side of a 20MP image (drag to compare):

<div class="compare_box">
<textarea readonly style="background-image:url(off.jpg)"></textarea>
<textarea readonly style="background-image:url(on.jpg)" ></textarea>
</div>
