# OpenDRT: Jed Smith's open display transform

this module is licenced GPLv3.

## connectors

* `input` format as defined by the `i gamut` and `i oetf` parameters
* `output` format as defined by the `o gamut` and `eotf` parameters

## parameters

* `i gamut` primaries of the input image
* `i oetf` transfer function of the input image
* `tn_Lp` display peak luminance: in nits. in sdr, the max value stays pinned at 100.0. in hdr, the max value is adjusted to match peak luminance in the hdr container.
* `tn_gb` grey boost: amount of stops to boost grey luminance, per stop of exposure increase of peak luminance. for example, if hdr grey boost is 0.1, middle grey will be boosted by 0.1 stops per stop of peak lumiance increase.
* `pt_hdr` hdr purity: how much to affect purity compression and hue shift as peak luminance increases. a value of 0.0 will keep the purity compression and hue shift behavior the same for sdr and hdr. a value of 1.0 will preserve more purity as peak luminance increases (at the risk of gradient disruptions in high purity high intensity light sources), and will reduce hue shift amount in highlights.
* `tn_Lg` display grey luminance: display luminance for middle grey (0.18) in nits. sets the target value for middle grey within the available luminance range of the display device.
* `crv en` tonescale overlay: draws a visualisation of the tonescale curve. a semi-log plot over ~ -0.005 to 128, with display encoding added.
* `tn_con` adjusts contrast or slope. a constrained power function applied in display linear.
* `tn_sh` shoulder clip: unitless control for the scene-linear value at which the tonescale system crosses the peak display linear value (1.0) and clips. this is not an exact constraint in order to keep the system simple, but corresponds to roughly 16 at shoulder clip = 0 and 1024 at shoulder clip = 1
* `tn_toe` quadratic toe compression. strongly compresses deep shadows. helpful to have some amount to smooth the transition into display minimum. higher values with a strong positive offset also valid. similar to common camera DRT tonescale strategies.
* `tn_off` pre-tonescale scene-linear offset. if 0.0, scene-linear 0.0 maps to display-linear 0.0 through the tonescale system. many camera imaging pipelines apply a negative offset to set the average of shadow grain at 0.0. a positive offset can be desireable to compensate for this and increase detail in shadows, in addition to being aesthetically desireable. offset should not be a negative number (Looking at you ACES 1.x)
* `tn_hc_en` enable contrast high: allows control of the upper section of the tonescale function. off by default, but can be useful if a stronger highlight contrast, or a softer highlight rolloff behavior is desired.
* `tn_hc` contrast high: amount adjust highlights. positive values increase highlight exposure, negative values decrease. 0 has no effect.
* `tn_hc_pv` contrast high pivot: amount of stops above middle grey (0.18) to start the adjustment.
* `tn_hc_st` contrast high strength: how quickly above the contrast high pivot the effect begins.
* `tn_lc_en` enable contrast low: contrast low adds contrast to the midtones and shadows. middle grey (0.18) is un-changed through the adjustment.
* `tn_lc` contrast low: amount of contrast to add. 0.0 has no effect. 1.0 will expose down by 1 stop at the origin (0,0)
* `tn_lc_w` contrast low width: the width of the adjustment. width below 0.5 will mostly affect values between 0 and middle grey. values above 0.5 will increasingly start to increase highlight contrast, which could be desired or not depending on what you are trying to do.
* `cwp` creative white: set the creative whitepoint of the display peak luminance. with D65 all channels are equal. with D50, the peak luminance value will match a D50 whitepoint. this can be creatively desireable. this adjustment is applied post-tonescale.
* `cwp_lm` creative white limit: limit the intensity range affected by the creative whitepoint. at 0.0, the entire intensity range is affected. as the limit is decreased, more of midtones and shadows are kept neutral. it can be creatively desireable to keep midtones more neutral while shifting highlights warmer for example.
* `rs_sa` render space strength: render space is the encoding in which the RGB ratios are taken. strength controls how much to desaturate from P3 gamut. creatively, the more you desaturate, the more brilliance is increased in the resulting image. to be used with caution as this affects every other aspect of the image rendering.
* `rs_rw` render space weight r: the red weight of the render space strength. modify with caution as this affects every other part of the image rendering.
* `rs_bw` render space weight b: the blue weight of the render space strength. modify with caution as this affects every other part of the image rendering.
* `pt_en` enable purity compress high: compresses purity as intensity increases. bare minimum functionality for a picture formation.
* `pt_lml` purity limit low: limit the strength of purity compression as intensity decreases, for all hue angles. a higher value will compress purity less in midtones and shadows.
* `pt_lml_r` purity limit low r: limit the strength of purity compression as intensity decreases, for reds. a higher value will compress purity less in midtones and shadows.
* `pt_lml_g` purity limit low g: limit the strength of purity compression as intensity decreases, for all greens. a higher value will compress purity less in midtones and shadows.
* `pt_lml_b` purity limit low b: limit the strength of purity compression as intensity decreases, for blues. a higher value will compress purity less in midtones and shadows.
* `pt_lmh` purity limit high: limit the strength of purity compression as intensity increases. can be helpful to keep some color in high intensity high purity light sources. use with caution as this can cause gradient disruptions.
* `pt_lmh_r` purity limit high r: limit the strength of red purity compression as intensity increases. can be helpful to keep some color in high intensity fire and pure red light sources. use with caution as this can cause gradient disruptions.
* `pt_lmh_b` purity limit high b: limit the strength of blue purity compression as intensity increases. can be helpful to keep some color in high intensity pure blue light sources. use with caution as this can cause gradient disruptions.
* `ptl_en` enable purity softclip: purity softclip increases tonality and smoothness in extremely pure input values that can not be adequately compressed into the display-referred gamut volume. the algorithm is tuned for common camera observer colorimetry sources.
* `ptl_c` purity softclip strength for cyan
* `ptl_m` purity softclip strength for magenta
* `ptl_y` purity softclip strength for yellow
* `ptm_en` enable mid purity: the mid purity module adjusts mid-range purity of midtones and highlights. without this module enabled, it is likely that midtones will not appear colorful enough, and highlights will appear too colorful resulting in chaulky pasty looking images especially in yellows and cyans.
* `ptm_lo` mid purity low: amount to increase purity of midtones and shadows in mid-range purity areas. a value of 0.0 will have no effect. a value of 1.0 is the maximum possible value while preserving smoothness.
* `ptm_lo_r` mid purity low range: the range of the mid purity low adjustment. higher values affect more of the luminance range.
* `ptm_lo_s` mid purity low strength: the strength of the mid purity low adjustment. higher values affect more of the purity range.
* `ptm_hi` mid purity high: amount to decrease purity of upper midtones and highlights in mid-range purity areas. a value of 0.0 will have no effect. a value of 1.0 is the maximum possible value while preserving smoothness.
* `ptm_hi_r` mid purity high range: the range of the mid purity high adjustment. higher values affect more of the luminance range.
* `ptm_hi_s` mid purity high strength: the strength of the mid purity high adjustment. higher values affect more of the purity range.
* `brl_en` enable brilliance: brilliance scales the intensity of more pure stimuli. the brilliance module is applied before the tonescale is taken for purity compression. this means that if you darken reds with this adjustment, the purity compression will also be reduced. this behaviour is natural and smooth.
* `brl` brilliance: global intenisty scale of high-purity stimuli.
* `brl_r` scale intensity of high-purity reds
* `brl_g` scale intensity of high-purity greens
* `brl_b` scale intensity of high-purity blues
* `brl_rng` brilliange range: as brilliance range is increased, the brilliance adjustments affect more the low intensity values of the image data.
* `brl_st` brilliange strength: as brilliance strength is increased, the brilliance adjustments affect more the low purity values of the image data.
* `brlp_en` enable brilliance post: post brilliance scales the intensity of more pure stimuli after purity compression hue shifts have been applied. with the OpenDRT algorithm it is possible to get high intensity high purity values going out of the top of the display-referred gamut volume, which can cause discontinuities in gradients, especially on the RGB primaries. this module can help compensate for this.
* `brlp` post brilliance: global post purity compression brilliance adjustment.
* `brlp_r` post brilliance r: scale intensity of high-purity red, post purity compression. this can help reduce rings or halos around high purity light sources.
* `brlp_g` post brilliance g: scale intensity of high-purity green, post purity compression. this can help reduce rings or halos around high purity light sources.
* `brlp_b` post brilliance b: scale intensity of high-purity blue, post purity compression. this can help reduce rings or halos around high purity light sources.
* `hc_en` enable hue contrast: hue contrast compresses hue angle towards the primary at the bottom end and expands the hue angle towards the secondary as intensity increases. it also increases purity as it compresses, and decreases purity as it expands. this leads to a nice creatively controllable simulation of this effect from per-channel tonescales. for OpenDRT we only keep the red hue angle control since it is the most useful.
* `hc_r` hue contrast r: amount to increase hue contrast at the red hue angle.
* `hc_r_r` hue contrast r range: hue contrast range control: determines where over the intensity range the hue contrast affects. higher values place the crossover point higher in the intensity range.
* `hsrgb_en` enable hue shift rgb: hue shift RGB adds hue distortion to the red green and blue primary hue angles as intensity increases. by default OpenDRT will compress purity in a straight line in RGB/chromaticity space. this can lead to perceived hue shifts due to the Abney effect, for example a pure blue will perceptually shift towards purple as it desaturates. to compensate for this, and to use as a creative tool, this module allows creative control over the path that red green and blue hue angles take as their purity is compressed.
* `hs_r` hue shift r: amount to distort the red hue angle towards yellow as intensity increases.
* `hs_r_r` hue shift r range: range of the red hue shift: higher values affect more of the lower intensity range.
* `hs_g` hue shift g: amount to distort the green hue angle towards yellow as intensity increases.
* `hs_g_r` hue shift g range: range of the green hue shift: higher values affect more of the lower intensity range.
* `hs_b` hue shift b: amount to distort the blue hue angle towards cyan as intensity increases.
* `hs_b_r` hue shift b range: range of the blue hue shift. higher values affect more of the lower intensity range.
* `hscmy_en` enable hue shift CMY: hue shift CMY adds hue distortion to the cyan magenta and yelow secondary hue angles as intensity decreases. this module allows some very minimal adjustments of secondary hue angles as a creative tool.
* `hs_c` hue shift c: amount to distort the cyan hue angle towards blue as intensity decreases.
* `hs_c_r` hue shift c range: range of the cyan hue shift: higher values affect more of the upper intensity range.
* `hs_m` hue shift m: amount to distort the magenta hue angle towards blue as intensity decreases.
* `hs_m_r` hue shift m range: range of the yellow hue shift: higher values affect more of the upper intensity range.
* `hs_y` hue shift y: amount to distort the yellow hue angle towards red as intensity decreases.
* `hs_y_r` hue shift y range: range of the yellow hueshift: higher values affect more of the upper intensity range.
* `clamp` clamp the output to the [0,1] range.
* `tn_su` surround: OpenDRT includes a simple surround compensation model. DCI cinema presets use dark surround. Rec.1886 uses dim surround. and sRGB display uses bright surround. this functionality should provide a better perceptual match between viewing environments.
* `o gamut` display gamut: the gamut of the display device. usually Rec.709 for sdr and either Rec.2020 (P3 Limited) for Rec.2100 / HDR10 or P3D65 for Dolby Vision.
* `eotf` the transfer function of the display device
