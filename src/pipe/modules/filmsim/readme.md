# filmsim: artic's sophisticated spectral analog film simulation

this is an implementation of Andrea Volpato's [spektrafilm](https://github.com/andreavolpato/spektrafilm/).
for a nice introduction see [this post](https://discuss.pixls.us/t/spectral-film-simulations-from-scratch/48209/1).

this module is best activated by applying the `filmsim.pst` preset. this will
take care of wiring the required input look up table files (shipped with the
vkdt installation). if you want to wire yourself, connect `data/spectra-em.lut`
and `data/filmsim.lut` to the `spectra` and `filmsim` connectors, respectively.

for self contained documentation, i'm summarising from [arctic's post](https://discuss.pixls.us/t/spectral-film-simulations-from-scratch/48209/1) here.

## the true color of film negatives

when researching film, the key takeaway is that the final colors depend heavily
on the second stage of the imaging process, whether it's the scanner's color
processing or the analog RA4 color reversal printing process. analog printing
seemed like the most authentic way to define the look, especially since
companies (primarily Kodak) spent decades refining it.

there are nice book chapters on simulating the full analog pipeline of color
photography [1,2,3]. film emulsions are quite sophisticated, relying on finely
tuned chemistry with silver halides, several dye couplers, and a pinch of
magic.

for anyone interested in film manufacturing, check out the series of videos by
SmarterEveryDay on Kodak ( How Does Kodak Make Film? series of 3, The Chemistry
of Kodak Film, Kodak's Film Quality Control Process).

## goal and motivation

the goal is to simulate the entire analog photographic process, from film
capture to the final print, using only the datasheets and basic knowledge.
to capture the look of products from Kodak and Fujifilm starting
from publicly available spectroscopic data. for example, Portra film and its
matching paper are designed to deliver subtle hue shifts and perfect contrast
for skin tones, while consumer films and paper are more saturated and
versatile. how much of these characteristics can we recreate from scratch?

while film simulation LUTs share similar goals, they often lack the flexibility
to be fine-tuned. in contrast, a fully physically based pipeline can better
reproduce the real-world versatility of the negative plus RA4 printing process
by offering adjustable parameters to tailor the final look. naturally, this
approach also brings along the inherent limitations of analog photography, so
you need to appreciate (or be nostalgic for) the analog process to embrace
these constraints.

## negative and print exposure

parameters: set `process` to `expose and scan negative` to output the negative,
`ev film` and `ev paper` are the negative exposure and print exposure

here are some test-strips to introduce the capability of the simulation. the
overall imaging process is split in two steps: negative and print. two
different exposures can be controlled, and color filters in the enlarger can
balance the colors of the print. here are virtual scans of Kodak Gold 200 at
different exposure compensations of the negative.

<img src="two_uncles_negative_exposure_ramp_gold_200_crystal_archive.png" style="width:100%"/>

the following strips are virtual prints on Fujifilm Crystal Archive Type II at
different print exposures (and constant good negative exposure).

<img src="two_uncles_print_exposure_ramp_gold_200_crystal_archive.png" style="width:100%"/>

raw file taken from this Play Raw Two Taiwanese uncles playing chess, thank you
@streetfighter.

## grain

parameters: `grain` `size` `uniform`

the simulation builds three sub layers for each channel, imitating modern color
negative films where each color layer is composed by 2-3 sublayers with
different sensitivity to increase latitude. the stochastic properties of each
layer and sublayers are imitated keeping into account that faster layers are
more noisy, i.e. they have larger particles.

<img src="grain_particle_area_ramp_portra_400_portra_endura.png" style="width:100%"/>

these above are a few strips of Kodak Portra 400 printed on Kodak Portra Endura
with vertical size of 1 mm. The average particle areas of the virtual silver
halide particles, then converted in dye clouds, is changed. in first
approximation, the area of the particles should be roughly proportional to the
ISO. in consumer films particles are in the range 0.2-2 micrometer diameter,
i.e. 0.03-3.2 micrometer squared.

here is an example with higher magnification crops with Kodak Portra 400 and
Kodak Portra Endura.

<table><thead><tr>
<th align="left">print</th>
<th align="left">negative</th>
</tr></thead>
<tbody>
<tr><td><img style="width:80%" src="print_016.png"/></td><td><img style="width:80%" src="neg_016.png"/></td></tr>
<tr><td><img style="width:80%" src="print_004.png"/></td><td><img style="width:80%" src="neg_004.png"/></td></tr>
<tr><td><img style="width:80%" src="print_001.png"/></td><td><img style="width:80%" src="neg_001.png"/></td></tr>
</tbody></table>

# saturation with DIR couplers

parameter: `couplers`

the level of saturation of the negatives is controlled via developer inhibitor
release couplers (DIR couplers). when substantial density is formed in one
layer, DIR couplers are released and can inhibit the formation of density in
nearby regions, both in the same layer and nearby layers. the diffusion in
nearby layers of DIR couplers produces increased saturation (loss of density on
the other channels, i.e. purer colors), also referred as interlayer effects.
here is an example using a signatureedits.com raw file, using Fujifilm
C200 and Fujifilm Crystal Archive Type II.

<img src="dir_couplers_ramp_car_fuji_c200_crystal_archive.png" style="width:100%"/>

# the filmsim data

to run this module, you need the `filmsim.lut` data file. it is shipped with
vkdt git and installed by default. the following steps are not necessary, but
if you want to create it yourself (and maybe play with different data points),
do this:
```
cd
git clone https://github.com/andreavolpato/spektrafilm
python -m venv spektra
source spektra/bin/activate
cd spektrafilm
pip install -e .
pip install dotmap  # used by mklut-profiles.py, not a spektrafilm dependency
cd src/spektrafilm/data/profiles
wget https://raw.githubusercontent.com/hanatos/vkdt/refs/heads/master/src/pipe/modules/filmsim/mklut-profiles.py
python ./mklut-profiles.py
tar cvJf filmsim.lut.xz filmsim.lut
mv filmsim.lut.xz path/to/vkdt/src/
```
the compressed file in `src/` is the one vkdt ships; `make lut` unpacks it into
`bin/data/filmsim.lut`, so there is nothing to copy by hand.

and in any case wire an `i-lut` module with filename `data/filmsim.lut` to the
`filmsim` input connector.

to update the film stock to new upstream data from spektrafilm,
a few steps are necessary:

* the new stock needs to be listed at the top of `mklut-profiles.py` before running the python script,
* `params.ui` should list the film and paper entries in the same order as in the script,
* the precomputed white balance values for the enlarger filters have to be computed for the new stock. see the top of `wb.h` for instructions on how to run the optimiser,
* if the new stock is a positive/reversal film, its direct-scan neutral also has to be fitted into `pos_wb.h` (same optimiser, see the top of that file); everything else gets a zero row there and is left unfiltered when scanned directly,
* the fit runs against the module defaults, couplers included; it assumes `g film` at 1.0 and preflash off,
* `glare` is pinned to 0 in the fit config and defaults to 0 in the module,
* if the total number of film stocks changed, this needs to be reflected in `head.glsl`, the line `const int s_paper_offset = ...; // first paper in data list/lut` has to equal `len(film_stocks)` in `mklut-profiles.py` (because the papers come right after the films in the same LUT). the film/paper counts in `main.c` derive from the `wb` array dimensions, so they follow automatically.

the LUT stores three 256-wide rows per stock, in this order: log sensitivity,
dye density, density-curve model. a mono (b/w) stock has its log sensitivity
replicated across rgb but its dye density goes in one channel with the other
two zeroed, since the spectral integral sums the three dye channels. both film
and print paper are developed through the parametric model, so no baked
density curve is shipped. The setup shader derives the row addresses from the
stock index and row type.

the regenerated `filmsim.lut` is checked into git as the compressed
`src/filmsim.lut.xz`, see the packing step above.

## connectors

* `input` scene referred linear rec2020 (after the colour module)
* `output` the exposed, developed, and printed film simulation (or negative)
* `filmsim` wire data/filmsim.lut with the film data
* `spectra` wire data/spectra-em.lut, the spectral upsampling table for emission

## parameters

this module has a lot of parameters. they are grouped into film options (first
block) and print paper options (second block).

* `process` determine the input and the output of the processing done here: (0) input raw image and output print on paper, (1) input raw image and output virtual negative (or, for a positive/reversal stock, the final scanned slide), (2) input scan of real film negative and output virtual print on paper.
* `film` the film id in the datafile
* `ev film` exposure correction when exposing the film
* `g film` gamma correction for exposing the film, use to adjust dynamic range
* `g fast` gamma correction for the fast (highlight) density curve sublayer
* `g slow` gamma correction for the slow (shadow) density curve sublayer
* `exhaust` developer exhaustion, reduces effective contrast/density in areas of high overall exposure
* `hl boost` boosts highlights above a threshold, useful in combination with halation.
* `paper` the paper id in the datafile
* `p base` scale on paper's base density in the print
* `ev paper` exposure correction when sensitising the paper, affects shadows more than the film exposure
* `g paper` gamma correction when sensitising the paper, affects dynamic range and contrast
* `g fast p` gamma correction for the paper fast density curve sublayer
* `g slow p` gamma correction for the paper slow density curve sublayer
* `p exh` paper developer exhaustion, reduces effective contrast/density in areas of high overall paper exposure
* `glare` veiling glare in the scanner/viewing optics, in percent of the illuminant
* `filter c` when exposing the print paper, dial in this share of cyan filter. this parameter is automatically filled by neutral optimisation. set to -1 to fill filter cmy with auto white balance weights for the current film and paper. when directly scanning a positive/reversal stock (process 1), this instead filters the scan lamp so the stock's own neutral point comes out grey, always auto-filled the same way (the slider is only shown for the print-negative process; use `tune m`/`tune y` to trim it for a direct scan)
* `filter m` same as `filter c`, magenta share. this parameter is automatically filled by neutral optimisation
* `filter y` same as `filter c`, yellow share. this parameter is automatically filled by neutral optimisation
* `tune m` fine tune the magenta filter (or, for a directly scanned positive stock, the scan lamp's magenta share). think of this as a red/green tint
* `tune y` fine tune the yellow filter (or, for a directly scanned positive stock, the scan lamp's yellow share). think of this as a warm/cold white balance temperature
* `preflash` switch preflashing the paper on or off, lowering maximum luminance and decreasing contrast
* `pf ev` exposure of the preflash step
* `pf m` magenta filtration during the preflash step, analogous to tune m
* `pf y` yellow filtration during the preflash step, analogous to tune y
* `couplers` switch developer inhibitor release couplers on or off (affects colourfulness and local contrast)
* `cp amt` amount of developer inhibitor release couplers
* `lang r` red-channel Langmuir isotherm coefficient for the coupler inhibition curve
* `lang g` green-channel Langmuir isotherm coefficient for the coupler inhibition curve
* `lang b` blue-channel Langmuir isotherm coefficient for the coupler inhibition curve
* `cp rad` radius of influence of the couplers, in micrometres on the negative
* `halation` switch halation on or off, causing a slight colourful blur around high contrast edges
* `radius` radius of the halation effect, in micrometres on the negative
* `hal amt` scale the rgb strength of the halation effect in lockstep
* `hal mids` midtone protection for halation. this heuristic lets you gradually fade out the effect of halation for darker tones. useful to preserve some extra sharpness outside the highlights. set to 1 for maximum effect, 0 means all tones are affected equally
* `hal bnc` number of halation light bounces to simulate
* `hal dec` decay factor applied to the halation contribution of each successive bounce
* `scat amt` in-emulsion light scatter: how much of the exposure is scattered before halation
* `strength` the strength of the halation effect per colour channel / layer in the film
* `grain` switch grain simulation on or off
* `size` scale the grain size. physical
* `uniform` scales the stock's own grain uniformity. 1.0 uses the stock value unchanged; higher uniformity suppresses grain in dense areas
* `enlarge` upsamples the image before exposing the paper, for a bigger print/export: careful with 4x, it requires a lot of memory!
* `scan ill` colour temperature (K) of the viewing/scanning illuminant. 5000 (D50) is the neutral reference setting
* `scne ill` colour temperature (K) of the light the scene was shot under, for correcting an unbalanced capture back to neutral. 0 is no correction; below 4000K it is modelled as a blackbody, at or above 4000K as CIE daylight.
* `film ill` colour temperature (K) to expose the film under instead of its own reference, for simulating a mismatched film/light combo (independent of `scne ill`). 0 is no effect (shot under the stock's own reference); below 4000K it is modelled as a blackbody, at or above 4000K as CIE daylight.

## licence

spektrafilm profile by Andrea Volpato, licensed under CC BY-SA 4.0.
redistribution and derivatives must credit the author, link the project
(https://github.com/andreavolpato/spektrafilm), preserve this license,
and remain CC BY-SA 4.0.
modifications must be noted.
[full text of the license and attribution requirements](https://github.com/andreavolpato/spektrafilm/blob/main/SPEKTRAFILM_LICENSE.txt)

### citation
if you use these profiles in your work, please cite
[the spektrafilm project](https://github.com/andreavolpato/spektrafilm),
see CITATION.cff for details.

### datasource
these profiles were created by processing raw measurement data from
data-sheets and/or scientific papers. original data are property of the respective holders.
film/photo-paper: kodak and fujifilm data-sheets, scientific publications, and technical material.
[reflectance: hisanari otsu](https://github.com/enneract/otsu2018),
[munsell](https://zenodo.org/records/3269912),
[human skin](https://www.nist.gov/programs-projects/reflectance-measurements-human-skin),
[forest colors](https://zenodo.org/records/3269920),
[japan colors](https://zenodo.org/records/5217752).
all data publicly available.

### modifications
the vkdt datafile lut has been generated from the spektrafilm `profile/*json`
using a [half assed python script found here](https://codeberg.org/hanatos/vkdt/src/branch/master/src/pipe/modules/filmsim/mklut-profiles.py).
the implementation has been done in a best effort kind of sense, with some
changes for efficiency (for instance the grain model was swapped out), as well
as ui and parameters (e.g. scale of enlarger filters). if you find gross
differences between spektrafilm and vkdt please let us know, this is probably
not intentional.
