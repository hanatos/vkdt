# grade: simple colour grading via lift gamma gain (ACES CDL)

this implements the popular and simple lift gamma gain scheme
to colour grade images.

note that it works in rgb and may result in out of gamut/spectral
locus colours.

this is in fact using the ACES colour decision list (CDL) formula for
slope/offset/power but i didn't rename the parameters because it's so similar.
also this does not work in log space nor in ACEScc colour space, so in a way
it's different anyways.

see: ACES S-2016-001 : ACEScct --- A Quasi-Logarithmic Encoding of ACES Data for use within Color Grading Systems

## parameters

* `lift` blend towards a color in shadows: `rgb * (1 - lift) + lift`. preserves white point.
  
* `gamma` power function for midtones: `rgb ^ (1 / gamma)`. gamma > 1 brightens, < 1 darkens.

* `gain` per-channel multiplication applied first. brightens midtones/shadows more than highlights.

* `offset` uniform addition to all channels, applied last. linear brightness adjustment.

* `mode` 0 = apply to all tones uniformly. 1 = apply selectively to shadows/midtones/highlights.
  
* `sh_pivot` in mode 1: controls shadow-to-midtone transition point. range 0-1.
  
* `hi_pivot` in mode 1: controls midtone-to-highlight transition point. must be > sh_pivot.

## color wheel constraints

the GUI uses a color wheel widget for lift, gamma, gain, and offset. the wheel enforces a zero-sum constraint on RGB deltas: R + G + B = 0 (pure chromaticity, no luminance change). this constraint creates an equilateral triangle in RGB space.

the maximum displacement any single component can achieve is **±0.317**, which occurs when one component is maximized and the other two balance it symmetrically. this geometric limit comes from the circular boundary of the wheel visualization in hexagonal coordinates.

the parameter ranges are set to exactly this geometric maximum (±0.317 for lift/offset, ±0.683–1.317 for gamma/gain). this ensures the visual wheel range matches the achievable parameter range with no clipping or wasted space.

## connectors

* `input` the display referred input image in zero/one range
* `output` the graded output image
