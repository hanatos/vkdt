# eq: local contrast equaliser

this module allows you to tweak contrast per frequency band. it uses a
decimated wavelet transform and variance estimation to shield edges from
artifacts. it's essentially unsharp mask (USM) on steroids.

it is intended to affect the *texture* of the image, not the edges. that is,
high-contrast detail such as hair and silhouettes will remain unaffected by
this module.

here is an example where the `eq` module is used for dehazing and for some
exaggerated contrast in the sky (image a playraw from pixls.us, pull the little
triangle at the bottom to compare):
<div class="compare_box">
<textarea readonly style="background-image:url(eq-off.jpg)"></textarea>
<textarea readonly style="background-image:url(eq-on.jpg)"></textarea>
</div>

## parameters

* `detail` modulate wavelet details at every scale, coarse (left) to fine (right), hold shift to lockstep

## connectors

* `input`
* `output`
