# kpn: kernel prediction neural network denoising

see [the `kpn-t` module](../kpn-t/readme.md) for the training part. the weights
lut produced by the latter module should be plugged into here.

this is mainly a proof of concept work that tiny mlp and cooperative matrices
work in vulkan, in the hope that the code will be useful in more interesting
applications. it's currently very rough code and only works for 16x16x16 f16
matrix multiplications with a subgroup size of 32 (i.e. nvidia devices RTX
20xx+, but amd and intel might follow).

all the credit for the index madness goes to thomas mueller for
[tiny-cuda-nn](https://github.com/NVlabs/tiny-cuda-nn) which i just ported to
vulkan/glsl.

## connectors

* `input` the noisy image to work on, in rgb
* `output` the denoised image
* `w` the weights for the neural network

## parameters
