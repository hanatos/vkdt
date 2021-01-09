# simple colour grading: lift gamma gain

this implements the popular and simple lift gamma gain scheme
to colour grade images.

note that it works in rgb and may result in out of gamut/spectral
locus colours.

## parameters

* `lift` a 3-vector for red green blue to lift the blacks
* `gamma` a 3-vector for red green blue to push the gamma (affects the mid tones mostly)
* `gain` a 3-vector for red green blue gain. this affects the whole image (but mostly pins the highlights) and works as rgb white balance coefficients would.
