# simple colour grading: lift gamma gain

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

* `lift` a 3-vector for red green blue to lift the blacks
* `gamma` a 3-vector for red green blue to push the gamma (affects the mid tones mostly)
* `gain` a 3-vector for red green blue gain. this affects the whole image (but mostly pins the highlights) and works as rgb white balance coefficients would.
