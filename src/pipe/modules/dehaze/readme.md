# dehaze: compute depth map from hazy landscape image and remove haze

computes a depth map by estimating the haze in the picture.
follows **Single image haze removal using dark channel prior**, He, Sun, Tang, CVPR 2009.
this works best for outdoors/landscape images where the airlight integral is mostly
constant throughout the image.

<div class="compare_box">
<textarea readonly style="background-image:url(dehaze-off.jpg)"></textarea>
<textarea readonly style="background-image:url(dehaze-on.jpg)" ></textarea>
</div>
<a href="https://discuss.pixls.us/t/play-raw-daylight-haze/10491/46">image by timur davletshin</a>

## connectors

* `input` linear scene referred data: the hazy landscape image
* `output` the image with haze removed
* `depth` the estimated optical thickness of the image

## parameters

* `radius` the blur radius used for the guided depth mask creation
* `epsilon` the edge threshold used for the guided depth mask creation
* `strength` strength of the dehazing effect. anything lower than 1.0 will leave residual haze in the distance.
* `t0` minimum optical thickness for numerical safety. a value greater than zero limits the effect of dehazing. usually you don't have to touch this.
* `haze` the airlight colour of the haze to remove. set to something like the brightest haze contribution in the input image.
