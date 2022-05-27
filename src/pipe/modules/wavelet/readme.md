# wavelet: selectively remove skin blemishes

this module is useful for selective skin retouching in the sense of
[pat david's classic article for gimp](https://patdavid.net/2011/12/getting-around-in-gimp-skin-retouching/).
it uses a wavelet decomposition and lets you remove detail at each scale
separately, constrained to a [drawn mask](../draw/readme.md).

for your convenience, there is the `retouch.pst` preset to add both a
`wavelet:retouch` and a `draw:retouch` module with connections to the default
pipeline (assuming `colour:01` is connected to `filmcurv:01` and inserting the
wavelets in between).

## parameters

* `scale` the amount of smoothing applied to each scale
* `exposure` the amount of exposure correction applied to the coarsest scale

## connectors

* `input` the input image
* `mask` connect the `draw` module here
* `output` the modified image
