# jddcnn: joined denoising and demosaicing convolutional neural network

this executes a network trained externally (tensorflow ipython notebook).
in the pipeline it should replace denoising and demosaicing, though it expects
the data normalised to [0, 1] range (can contain negative values in case of noise).

# TODO
* finish wiring in main.c
* b/w scaling, upload right values for input

compat:
* replace NV by coopmat wrapper

for optimisation:
* put rescaling/gainmap in ingest?
* sort max pooling operations to *output* to reduce global memory traffic
* main loop over channels: use workgroup dimension z to parallelise more for more channels?
* uvec4 loads (though i think i don't want to be doing this)

