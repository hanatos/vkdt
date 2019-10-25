# highlight reconstruction

this works only for raw images, and on raw data.

it runs in two steps: blur and combine.

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

this is done by reduce.comp and assemble.comp. the last step, going from xtrans/bayer to rgb image
is done in a halfsize scaling kernel half.comp/doub.comp
