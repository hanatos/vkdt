# eq: local contrast equaliser

this module allows you to tweak contrast per frequency band.
it uses a decimated wavelet transform and variance estimation
to shield edges from artifacts.

here is an example where the `eq` module is used for dehazing and for some
exagerated contrast in the sky (image a playraw from pixls.us, pull the little
triangle at the bottom to compare):
<div class="compare_box">
<textarea readonly style="background-image:url(eq-off.jpg)"></textarea>
<textarea readonly style="background-image:url(eq-on.jpg)"></textarea>
</div>

## parameters

* `detail` modulate wavelet details at every scale, fine to coarse

## connectors

* `input`
* `output`
