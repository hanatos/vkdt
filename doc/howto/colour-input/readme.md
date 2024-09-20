# how to characterise the colour response of your camera

vkdt mainly uses the D65 colour matrix for this. it is extracted from
the metadata shipped with the raw file.

more colour accuracy can be gained by:
* using a better input lut profile/clut
* performing colour correction with a target such as a ColorChecker

## using colour profiles/luts

more accurate spectral input profiles can be created if you have
spectral response functions of the colour filter array. if you don't,
vkdt ships a tool to reconstruct plausible curves which will still
behave better than the matrix: gamut boundaries will be handled
gracefully without creating imaginary stimuli/negative energy, and
interpolation of the input transform for white balancing can be
performed more accurately. this interpolation will show as a temperature slider
in the `colour` module. it is similar in spirit to the interpolation done in
adobe DCP profiles.

the processing of all of this is done in the [`colour` module](../../../src/pipe/modules/colour/readme.md) and the [documentation for the
tool to create spectral lookup tables is found here](../../../src/tools/clut/readme.md).
in short, it creates a spectral lut from a DCP profile or the same data
embedded in a DNG file.

tl;dr:
```
vkdt-mkssf your.dcp
vkdt-mkclut <camera model>
```

there are a few options for the optimisation process involved here
and there is an instructive report printed as html.

### contributing an input profile lut

once you created a lut, are happy with the results and want to share with
others, please submit a pull request to [the `camconst` data
repository](https://github.com/hanatos/vkdt-camconst)!

## using a colour checker

sometimes you don't have spectral data for your cfa nor a dcp profile or a dng that
contains the good information. but maybe you own a colour checker with 24 patches?
in this case you can take a picture of one, and crop it to fill the frame, like so:

[![cc-in](cc-in.png)](cc-in.png)

you can use the perspective correction in the `crop` module to make it stand upright in
the frame. the little angle brackets printed on the checker should mark the corners
of the frame now.

you'll notice it looks a little blueish. from this picture, you can create a
correction function (via radial basis functions) that will map all the 24
patches exactly to their reference value. for this, we'll need to pick the
patches and we'll need the reference values. for both of these tasks, vkdt comes with
the `ColorChecker.pst` and the `SpyderChecker24.pst` preset (you can create
your own from an argyll .cht file with the `vkdt scanin` tool). if you press
`ctrl-p` in darkroom mode this will trigger the default hotkey for applying a
preset, so you can select `ColorChecker.pst`.

this preset contains the spot positions and reference values from the
`ColorChecker.cht` shipped with argyll. your graph should now contain a `pick:target`
module connected like so:

[![pick-graph](pick-graph.png)](pick-graph.png)

make sure you left the parameters of the `colour` module at their default before
doing this. in particular leave `mode` at `no rbf`, or else the colour picker
will not grab vanilla source values now.

you should see the picker grabbed colours like these:

[![pick-ui](pick-ui.png)](pick-ui.png)

if that is the case it's now safe to leave the `grab` combo box at `only on change`.

you can now use these patches for correction in the `colour` module. for this,
use the `import` button in the `colour` module's ui. make sure the argument is
set to `target` which is the instance id of your colour picker.

if you now set the `mode` in the `colour` module to `use rbf`, you should see
the corrected output in the main window and an indication of the corrected patches
in the ui of the module, like so:

[![cc-out](cc-out.png)](cc-out.png)

if you have done this for a specific lighting for a certain shoot, you can now
copy/paste the settings of the colour module to several images. it may be a
good idea to create a temporary preset for this (default: `ctrl-o`, and then
select only the parameters to the `colour` module) so you can quickly apply it
to other images. you can also remove the `pick` module again now and simply use
`ctrl-c` `ctrl-v` in lighttable mode.
