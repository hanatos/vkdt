# highlight reconstruction

this module supports inpainting of clipped channels of raw images.
it works only for raw images, and on raw data.
a kind of wavelet pyramid is used to obtain the inpainted colour,
trying to keep colour ratios smoothly varying.

## parameters

* `white` this lets you manually overwrite the clipping point. ideally  
  everything would be normalised to clip at 1.0.
* `desat` controls the amount of desaturation of highlights. it is a parameter
  because there are cases for both extremes. some images come with chromatic
  aberration artifacts near clipped highlights and would thus only blur false
  colour magenta into the clipped region. you want to desaturate this
  completely. however, things such as saturated skin colours should be
  inpainted with the original skin tone, i.e. not be desaturated at all.
* `soft` controls the amount of *softness* of the inpainting. essentially this weighs
  different scales of the wavelet pyramid. no softness means the highest frequencies
  get a good share of the result. all soft means pretty much only the coarsest scale, i.e
  close to constant colour reconstruction. again, when inpainting chromatic aberration artifacts
  you may choose to go soft, but when reconstructing a smooth gradient of a sunset you want
  a specific kind of gradient.


## technical

reconstruction runs in two steps: blur and combine.

the blur is clipping aware, i.e. it disregards clipped pixels and renormalises
results to include only unclipped pixel values.

recombination then takes the original image and the blurred result as
input images. it rescales the blurred version in brightness to match
any potentially available unclipped pixels.


gauss pyramid:

* downsize half size, disregard clipped pixels (all clipped -> output clipped)
* blur similar to llap, but disregard clipped pixels

recombine:
* upsample low res version
* compare to hi res: rescale low res to match hi res unclipped channels
* fill in

this is done by `reduce.comp` and `assemble.comp`. the last step, going from xtrans/bayer to rgb image
is done in a halfsize scaling kernel `half.comp`/`doub.comp`
