# how to characterise the colour response of your camera

vkdt mainly uses the D65 colour matrix for this. it is extracted from
the metadata shipped with rawspeed or contained in the dng exif tags
of the raw file.

more accurate spectral input profiles can be created if you have
spectral response curves of the colour filter array. if you don't,
vkdt ships a tool to reconstruct plausible curves which will still
behave better than the matrix: gamut boundaries will be handled
gracefully without creating imaginary stimuli/negative energy, and
interpolation of the input transform for white balancing can be
performed more accurately.

the processing of all of this is done in the [`colour` module](../../../src/pipe/modules/colour/readme.md) and the [documentation for the
tool to create spectral lookup tables is found here](../../../src/tools/clut/readme.md).
in short, it creates a spectral lut from a dcp profile or the same data
embedded in a dng file.

tl;dr:
```
vkdt-mkssf your.dcp
vkdt-mkclut <camera model>.txt
```

there are a few options for the optimisation process involved here
and there is an instructive report printed as html.

## contributing an input profile lut

once you created a lut, are happy with the results and want to share with
others, please submit a pull request to [the `camconst` data
repository](https://github.com/hanatos/vkdt-camconst)!
