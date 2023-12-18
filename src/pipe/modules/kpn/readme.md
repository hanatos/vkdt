# kpn: kernel prediction neural network denoising

see [the `kpn-t` module](../kpn-t/readme.md) for the training part. the weights
lut produced by the latter module should be plugged into here.

## connectors

* `input` the noisy image to work on, in rgb
* `output` the denoised image
* `w` the weights for the neural network

## parameters
