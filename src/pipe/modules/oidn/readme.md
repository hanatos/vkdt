# oidn: open image denoise neural nework

the spir-v here is automatically generated from the oidn onnx.

this module requires a `data/oidn-weights.dat` file containing
the pre-trained weights for the oidn light ldr network.

if the input exposes some kind of inter-pixel correlation (such as from a
preceeding demosaicing operation) this module will not do anything at all. oidn
will classify the correlated noise statistics as signal and pass them on
unchagned. if you have a very noisy mosaic image, use `half size` as
demosaicing method to keep pixels uncorrelated. avoid resampling nodes
before denoising.

## parameters

* `arch` select the gpu achitecture. auto means use the vendor of the current gpu. others may or may not work on your device.
* `model` select the oidn network model

## connectors

* `input` scene-referred data, preferrably low-res and in [0,1] 
* `output` the denoised image

## ethics

it is my understanding that oidn has been trained on purely synthetic data,
output from rendering systems. thus, no internet scraping or violation of
rights of third parties took place during training.
