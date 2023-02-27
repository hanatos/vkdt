# how to find things if you know darktable

in general `vkdt` uses a linear rec2020 encoding for the intermediate colour
buffers, and there is pretty much no Lab code at all (except to compute delta E
for display/loss). so it's really up to the user what will be scene-referred,
by default most of the modules are (but you can of course put your display
transform wherever you want).

i'm skipping input/output modules because dt doesn't work this way (dt's
input/output is more coupled to the core than to the processing pipeline).

there are also various other things like histograms/waveforms/displays/colour
pickers/noise profiling/gamut checking which are implemented as modules in vkdt
and thus don't have a direct counterpart in dt as a module.

there's an obvious difference for drawn masks (via brush strokes/pentablet in
`vkdt` and via parametric shapes or conditional blending in dt).

dt's snapshots probably correspond most closely to the `ab` module, which
compares two options in a single active graph.

and of course dt has none of the 3d rendering/movie/animation/keyframing
functionality. about the rest:

## approximate correspondence to darktable modules

darktable module     | `vkdt` module           | note
---------------------|-------------------------|----------------------------------
chromatic aberrations| `ca`                    | different tech
demosaic             | `demosaic`              | different tech, works transparently for xtrans/bayer
highlight reconstruction| `hilite`             | uses inpainting by default
color lookup table   | `colour`                | uses same rbf code 
color calibration    | `colour`                | use cc24 presets+colour pickers
input color profile  | `colour`                | can ingest DCP profiles or spectral response curves (not icc)
white balance        | `colour`                | DCP dual wb, finetune via rgb sliders
exposure             | `colour`, `exposure`    |
monochrome           | `colour`                | set saturation to zero, map colours before using rbf
color balance rgb    | `grade`                 |
crop                 | `crop`                  |
rotate and perspective| `crop`                 |
sharpen              | `deconv`, `eq`, `contrast`| none of them is a direct match
denoise (profiled)   | `denoise`               | wavelet part and noise profile application are similar (noise profiles not compatible since they are applied pre-blackpoint subtraction in `vkdt`)
lens correction      | `lens`                  | simpler model that also fits fisheye and anamorphic, no lensfun
contrast equalizer   | `eq`                    | overall similar wavelet approach but with different edge protection measures
diffuse or sharpen   | `deconv`,`eq`           | for the sharpen part, only isotropic diffusion for the blurring part
sigmoid              | `filmcurv`              | using jandren's idea of applying the point process CDF, [using pretty much unchanged dt UCS code by aurelien](https://github.com/hanatos/vkdt/blob/master/src/pipe/modules/shared/dtucs.glsl)
graduated density    | `grad`                  |
local contrast       | `llap`                  | straight adaptation of the same local laplacian pyramid code (but in rgb, not Lab)
shadows and highlights| `llap`                 | same functionality, disjoint tech
vignetting           | `vignette`              | smoother falloff, dt's version has a discontinuity
tone equalizer       | `zones`                 | uses same core tech (guided filter/luminance quantisation)
negadoctor           | `negative`              | same formula but the post grading is in `colour` and `grade`
framing              | `frame`                 |
retouch              | `wavelet`, `inpaint`    |
space invaders       | `quake`                 |

