# kpn-t: fully fused multi layer perceptron for denoising

this is some super experimental neural network training module, the inferencing
only part is in [the `kpn` module](../kpn/readme.md).
if you don't know better already, you probably don't need to read on :)

## training

put the following in a file like `train-mlp-cli.cfg`:
```
frames:150001
fps:0
module:i-pfm:main
module:i-pfm:ref
module:cnngenin:01
module:kpn-t:01
module:o-lut:w
module:display:main
connect:i-pfm:main:output:cnngenin:01:imgi
connect:i-pfm:ref:output:cnngenin:01:refi
connect:cnngenin:01:imgo:kpn-t:01:input
connect:cnngenin:01:refo:kpn-t:01:ref
connect:kpn-t:01:w:o-lut:w:input
connect:kpn-t:01:output:display:main:input
param:i-pfm:main:filename:img_12800.pfm
param:i-pfm:ref:filename:img_100.pfm
param:cnngenin:01:generate:1
param:kpn-t:01:init:1
param:kpn-t:01:L2:0
param:o-lut:w:filename:mlp-w
```

and then run something like:
```
$ vkdt cli -g mlp/train-mlp-cli.cfg --last-frame-only
```

leaky relu works better with reduced learning rate of `alpha:0.001` but relu leads to dead neurons.
softmax activation for the convolution kernel also helps stability. the combination of both measures
lets us work with leaky relu without exploding gradients.

## TODO

FIXME: can't compute `DEBUG_DERIV` on current softmax code
FIXME: loss plot seems fubared

release:
TODO: use KHR coopmat extension instead of NV (mainly types matA matB matC)
TODO: compile both versions (bck,fwd,inf)x(cm,nocm)
TODO: pick kernel based on flag in qvk
TODO: what about the non-32 subgroups? (is a vk1.3 extension)
TODO: what is good input? (paris/durand propose shipping noise sigma)
TODO: luma/chroma blend

training:
TODO: input luminance + noise std dev (for rendering)
TODO: generate noisy input synthetically from noise profiles
TODO: cnngenin: train with ref vs. all other iso

extra credits: run on raw bayer and demosaic as you go?


## parameters

* `alpha` adam: the learning rate for the optimiser
* `beta1` adam: the exponentially moving average weight for the first moment
* `beta2` adam: the exponentially moving average weight for the second moment
* `eps`   adam: the value to avoid division by zero
* `init`  how to initialise weights in the first iteration
* `L2`    how much L2 regularisation to apply to the weights (0 means none)

## connectors

* `input` the input pixels to convolve by the optimised kernel
* `ref` the reference image to match
* `output` the rendered image after application of the kernel convolution
* `w` the weight buffer to be written out after convergence
* `graph` a log-space error graph for visualisation of the convergence behaviour
* `vis` visualisation of the weights and their derivatives
* `debug` ever changing dev wire during coding


## model

inferencing looks like

![architecture](arch.svg)

where
* `mip` generates 3 levels of mip maps in shared memory in one kernel call `M0` -> `M1`, `M2`, `M3`
* `fwd` evaluates a fwd MLP based on 15 pixels/2 channels context in a weird
  star shaped spatial kernel, outputs 15 kernel weights + one alpha blend
  weight, `M`,`w` -> `K` (also write `A` auxiliary internal activations for backprop)
* `apply` takes this kernel and convolves the input `M`, `K` -> `I`
* `up` performs upsampling on the coarse input and blends alpha * the high res on top. `I`, `Oc` -> `O`

`fwd` reads the MLP weights `w` straight from file or from adam and outputs
`K` kernel weights (16 values: 15 for the spatial convolution and 1 for the blend
weight). the spatial support is little better than 3x3 and looks like:
```
  x
xxxxx
 xxx
xxxxx
  x
```
the `apply` module and the `up` module read the kernel weights (for convolution and blending, respectively).
the weights as input to `fwd` are shared between all mip map levels.

we pass alpha in the 4th channel, it makes
backprop easier by separation of concerns


## backpropagation

need some names for the buffers for sanity.

* `w` MLP weights
* `K0-3` kernel convolution weights (including alpha as 16th entry)
* `O0-2` output of upsampled/blended `up` node
* `I0-3` convolved image, output of `apply`
* `M0-3` input image + 3 mipmaps

the partial derivatives `dEdw` of the loss `E` by the weights `w` are propagated backwards through the system
by feeding the derivative of the loss by output image pixel `dEdO` to a number of modules which are cascaded
across the 4 levels:

* `dup` derivative of blending/2x2 upscaling: `dEdO` -> `dEdI`, `dEdOc`
* `dapply` derivative of MLP output kernel: `dEdI` -> `dEdK`
* `bck` backpropagation inside MLP: `dEdA` (`A`: activation)
* `mulw` sum partial weight derivatives shared between pixels

and all `dEdw` on all levels are summed up (weight sharing)

### dup

upscaling/blending passes on the derivative backwards to the convolved images
on this level `I` and coarser `Oc`, both `rgb` and their alpha values
`a`. `up` uses a 6x6 bspline filter (consider weights in the sum).

`dEdO`,`I`,`Oc` -> `dEdI`, `dEdOc`

`O` = `I.rgb` * `I.a` + (1-`I.a`) * sum6x6(`Oc`)

`E(O)` = `E(I.rgb * I.a + (1-I.a) * sum6x6(Oc))`

`dEdI.rgb` = `dEdO` * `dOdI.rgb` = `dEdO` * `I.a`

`dEdI.a` = `dEdO` * `dOdI.a` = `dEdO` * (`I.rgb` - sum6x6(`Oc.rgb`))

`dEdOc.rgb` = `dEdO` * sum6x6(1-`I.a`)

`dEdOc.a` = 0

the alpha of the coarse is applied one step further down the cascade and not important here.

### dapply

the derivative of convolution by the kernel is simply summing up the taps of
the filter (to determine derivative by kernel weight, sum up the values of the
input mip maps `M`):

`dEdI`,`M` -> `dEdK`

`I.rgb` = sum(`K` * `M.rgb`)
`I.a` = `K`

`dEdK` = `dEdI.rgb` * `dIdK` = `dEdI.rgb` * `M.rgb`

where we have one `dEdK` for each of the 15 taps of the filter. the 16th output is passed on:

`dEdK` = `dEdI.a`

### bck

the MLP backpropagation takes kernel weight derivatives (`K`) and maps them to
MLP activation derivatives (`dEdA`).

### mulw

because of weight sharing, the MLP weights `w` are the same
for all pixel inputs. so we'll sum up the derivatives after computing

`dEdw` = `dEdA` * `A`

here `A` are the activations of the layers of the `fwd` eval pass, and `dEdA`
are the partial derivatives by activation computed by the `bck` pass.

### sum

this finally sums all the `dEdw` computed by `mulw` for the four mip levels.


## testing/debugging

finite differences for many weights (define `DEBUG_DERIV` in `config.h`)
we'll feed forward and compute the loss in an animation, one frame for each weight.

finite differences for `dEdw`: for each frame, pick one `w`. permute it by some epsilon, compute loss.

FIXME: seems the recent updates broke this (diff version doesn't output useful stuff any more)

try these
```
# with DEBUG_DERIV in config.h:
make debug -j20 && vkdt cli -g mlp/mlp.cfg --format o-pfm --filename diff --last-frame-only --output view0
# without DEBUG_DERIV in config.h:
make debug -j20 && vkdt cli -g mlp/mlp.cfg --format o-pfm --filename deriv --output view0 --config frames:1
```

## extension to pre-bayer/xtrans

mosaic input is better because it's less data and doesn't require a demosaicing
pass on noisy data. probably: add one more layer to the 4 colour mips: mosaic
full res. for bayer, there are two different sets of kernels to be predicted:
(i) for green and (ii) for red/blue photo sites. in demosaicing algorithms,
green/red "colour" sites are treaded the same with respect to gradient
prediction and filter weight. this means for all intents and purposes green
on a red line and green on a blue line can be treaded by the same, and
similar for the colour pixels.

xtrans: not so sure. probably need to work with explicit rotation/flip symmetry
of "corner green" (4x corner) and "up colour" (4x sides) and an additional
kernel for "center green".

note that these weights cannot be reused for the colour mipmaps. that is, we'll
end up with 3 sets of weights for bayer and 4 for xtrans.
(or maybe even 3x the mosaic weights since they differ for colours?)

while at it, also train against iso100 demosaiced + deconvolved for optical low
pass filters.
