# filmsim: artic's sophisticated spectral analog film simulation

this is based on [agx emulsion](https://github.com/andreavolpato/agx-emulsion).
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

parameter: set `enlarge` to `2x` or `4x` for magnification. beware of `4x`, it requires a looot of GPU memory!

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
git clone https://github.com/andreavolpato/agx-emulsion
python -m venv agx
source agx/bin/activate
cd agx-emulsion
pip install -r requirements.txt
pip install -e .
cd agx_emulsion/data/profiles
wget https://raw.githubusercontent.com/hanatos/vkdt/refs/heads/master/src/pipe/modules/filmsim/mklut-profiles.py
python ./mklut-profiles.py
mkdir -p ~/.config/vkdt/data
cp filmsim.lut ~/.config/vkdt/data
```

and in any case wire an `i-lut` module with filename `data/filmsim.lut` to the
`filmsim` input connector.

to update the film stock to new upstream data from agx-emulsion,
a few steps are necessary:

* the new stock needs to be listed at the top of `mklut-profiles.py` before running the python script,
* `params.ui` should list the film and paper entries in te same order as in the script,
* the precomputed white balance values for the enlarger filters have to be computed for the new stock. see the top of `wb.h` for instructions on how to run the optimiser,
* if the total number of film stock changed, this needs to be reflected in `head.glsl`, the line `const int s_paper_offset = 15; // first paper in data list/lut` has to be the number of films (because the papers come right after that in the same file).

finally, the new `filmsim.lut` needs to be checked into git, which is done by creating
the compressed version and moving it to the `src/` directory:
```
tar cvJf filmsim.lut.xz filmsim.lut
mv filmsim.lut src/
```

## connectors

* `input` scene referred linear rec2020 (after the colour module)
* `output` the exposed, developed, and printed film simulation (or negative)
* `filmsim` wire data/filmsim.lut with the film data
* `spectra` wire data/spectra-em.lut, the spectral upsampling table for emission

## parameters

this module has a lot of parameters. they are grouped into film options (first
block) and print paper options (second block).

* `process` determine the input and the output of the processing done here: (0) input raw image and output print on paper, (1) input raw image and output virtual negative, (2) input scan of real film negative and output virtual print on paper.
* `film` the film id in the datafile
* `ev film` exposure correction when exposing the film
* `g film` gamma correction for exposing the film, use to adjust dynamic range. hidden from gui by default
* `grain` switch grain simulation on or off
* `size` scale the grain size
* `uniform` uniformity of the grains as seen through the pixels. 1.0 means no variation at all
* `paper` the paper id in the datafile
* `ev paper` exposure correction when sensitising the paper, affects shadows more than the film exposure
* `g paper` gamma correction when sensitising the paper, affects dynamic range and contrast. hidden from gui by default
* `enlarge` resize the image when exposing the paper: careful with 4x, it requires a lot of memory!
* `filter c` when exposing the print paper, dial in this share of cyan filter. this parameter is automatically filled by neutral optimisation. set to -1 to fill filter cmy with auto white balance weights for the current film and paper
* `filter m` when exposing the print paper, dial in this share of magenta filter. this parameter is automatically filled by neutral optimisation
* `filter y` when exposing the print paper, dial in this share of yellow filter. this parameter is automatically filled by neutral optimisation
* `tune m` fine tune the magenta filter. think of this as a red/green tint
* `tune y` fine tune the yellow filter. think of this as a warm/cold white balance temperature
* `couplers` amount of developer inhibitor release couplers (affects colourfulness and local contrast)
* `cp rad` radius of influence of the couplers, as a fraction of the longer side (width or height)
* `halation` switch halation on or off, causing a slight colourful blur around high contrast edges
* `radius` change the radius of the halation effect
* `strength` the strength of the halation effect per colour channel / layer in the film
* `scale` scale the rgb strength of the halation effect in lockstep
* `hal mids` midtone protection for halation. this heuristic lets you gradually fade out the effect of halation for darker tones. useful to preserve some extra sharpness outside the highlights. set to 1 for maximum effect, 0 means all tones are affected equally.
