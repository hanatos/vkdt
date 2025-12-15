# how to reduce image noise

## image denoising

standard noise reduction is done by the
[default denoise module](../../../src/pipe/modules/denoise/readme.md).
it makes use of a noise model to use as a prior for the expected amount of
noise in the image, depending on iso value. this model can be tweaked manually
by changing the `noise a` and `noise b` parameters in the
[raw input module](../../../src/pipe/modules/i-raw/readme.md).
just up the sliders until the perceived amount of noise in the image is
lowered enough while still preserving good detail.
of course a more principled approach is to measure the amount of noise
and use fitted values.

## noise profiling

in short, these are the instructions for the impatient:

```
vkdt noise-profile your-raw-file.raw
```

it will write the noise profile for the camera maker/model/iso combination to
`data/nprof/` so it can be picked up by `vkdt`.

for various things it is useful to know how much noise we can expect from a
pixel in the raw image file. we fit a gaussian/poissonian mixture to the
observed variance in an image.

to derive the values, there are two processing graphs in the `data/` directory:

```
data/noiseprofile.cfg
data/noisecheck.cfg
```

to wire these to your specific raw file, there is a convenience wrapper bash
script, `vkdt-noise-profile`. it takes a single argument, the raw file
you'd like to measure. it will output an nprof file, copy it to the
`data/nprof` directory, and run the noisecheck graph, resulting in an
output histogram such as the following:

![rawhist](noisehistogram.jpg)

this is a raw histogram in log/log space. the red bars indicate the observed
noise, and the white line is the resulting fit. there is some outlier rejection
mechanism in place that tries to separate signal from noise variance, so the
white line should not blindly match the red bars but take a consistent path
around the minimum of the observed variance (observed is the sum of noise and
signal variance).

note that the same considerations with regard to good profiling shots hold as
they did for darktable previously (cover all dynamic range, out of focus). on
the other hand the new outlier rejection scheme seems to be a lot more robust,
and profiling the raw data has the advantage that *black stripes* outside
the crop window contribute to a good estimate of the gaussian portion. as a
result i could pretty much use any shot i wanted to denoise out of the box for
single-shot noise profiling, without the need for dedicated profiling shots.

## convenience gui

for quick and dirty profiling of single shots, `vkdt` supplies a preset:
`noise-profile.pst`. apply this on the graph (press `ctrl-p` in darkroom or
node editor view), it will add a raw histogram (`rawhist` module) as well as a
[noise profiling module `nprof`](../../../src/pipe/modules/nprof/readme.md).
this `nprof` module contains two buttons
that allow you to test the newly created profile locally as well as install it
for later automatic application in your home directory. the raw histogram
can be used to inspect the quality of the profile as described above.

you want to remove the `rawhist` module from the `i-raw` module after finishing
the profiling process for performance reasons. there is a third button in the `nprof`
ui to accomplish just that.

## contributing noise profiles

if you created a good profile for your camera and want to share with others,
please submit a pull request to [the `camconst` data repository](https://github.com/hanatos/vkdt-camconst)!
