# denoising

this shall read a noise profile, make good use of the black/noise estimation stripe
in raw images, and then run some sort of Fisz-transform wavelets for noise reduction.

currently it is unimplemented and all it does is subtract the black point
and cut away the black border. these are essential tasks for the raw pipeline,
so the module is still in the pipeline by default.
