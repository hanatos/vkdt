# colour picker

attach this module to the output of any other module. it will collect
statistics for the box you selected in the gui. it is possible to collect up to
20 boxes simultaneously (after that you'll need to attach another instance of
this module).

the picked colour will appear as parameter of this module, so you'll be able
to copy/paste it from the `.cfg` file afterwards.

note that the gui will only instant-update the picked colour if the runflags
for the graph contain *download sink*.

## parameters

* `nspots` number of picked spots
* `spots` geometric coordinates of the picked spots in the image
* `picked` actually picked colour values (3x `nspots` floats, rec2020)
* `ref` reference values, to be set by the user in the `.cfg` file (rec2020)
* `show` 0 - picked, 1 - ref, 2 - diff
* `grab` indicates whether the results will be grabbed all the time (not only during regular pipeline reruns)
* `de76` a dummy parameter that gets filled with the CIE DE76 error metric between `picked` and `ref`

note that the format of `nspots` and `ref` is the same as for
the display of reference values in the [CIE xy diagram plot](../ciediag/readme.md).

## connectors

* `input` input buffer to pick colour from
* `spectra` (optional) connect spectra.lut for spectral upsampling
* `dsp_` (optional) will be named 'dspy' once 'spectra' are connected
* `dspy` (optional) outputs spectral reconstructions of picked colour
* `picked` outputs the picked data for further use by other modules
