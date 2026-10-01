# okgmtclp: experimental gamut clipping in oklab space using Bjorn Ottosson's methods

this module implements Björn Ottosson's Oklab gamut clipping methods
https://bottosson.github.io/posts/gamutclipping/. Some additional 
commentary can be found on [Simon's Tech Blog](https://simonstechblog.blogspot.com/2021/05/studying-gamut-clipping.html)

this module tries to clip out-of-gamut colors by pulling them back into gamut
along a perceptually uniform line.

## parameters

* `mode` projection method. preserve chroma or project
* `L0` luminance point to project to. L0=0.5 is hue independent. L0=L_cusp will project to the luminance value equal to that of the cusp of the hue
* `project` projection target. single point will project all out-of-gamut points to the same target. adaptive blends between pure chroma compression and projection towards a single point
* `alpha` only applicable when the projection is adaptive
* `dspyl` selected luminance value for the dspy preview. ignored if picked input is connected
* `dspyc` selected chroma value for the dspy preview. ignored if picked input is connected
* `dspyh` selected hue value for the dspy preview. ignored if picked input is connected

## connectors

* `input` the input in rec2020 rgb
* `output` the output in rec2020 rgb
* `picked` the color to plot on the dspy
* `dspy` a visualization of the oklch h (hue) slice of the picked color and where it is getting clipped to. x-axis is C (chroma) and y-axis is L (luminance)

## technical

a large shortcoming of this method is that the oklab color space
does not accurately map imaginary colors. in other words, colors that 
fall outside the spectral locus or above visibly possible are meaningless
and thus may get clipped to a color that in not of similar hue or chroma.
though it has similar shortcomings, you can consider the colour module's 
built in gamut mapping if this module is not working for your use case.

## credits

Simon Yeung's Shader visualization https://www.shadertoy.com/view/7tlGRS

Björn Ottosson's Oklab gamut clipping methods: https://bottosson.github.io/posts/gamutclipping/