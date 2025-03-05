# filmsim: artic's sophisticated spectral analog film simulation

this is based on [agx emulsion](https://github.com/andreavolpato/agx-emulsion).
for a nice introduction see [this post](https://discuss.pixls.us/t/spectral-film-simulations-from-scratch/48209/1).

to run this, you need the `filmsim.lut` data file. to create it:

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

and then wire an `i-lut` module with filename `data/filmsim.lut` to the `filmsim` input connector.

TODO
* put documentation/nice example images from artic's post above
* implement halation
* map grain params to film iso values

## connectors

* `input` scene referred linear rec2020 (after the colour module)
* `output` the exposed, developed, and printed film simulation
* `filmsim` the filmsim.lut with the film data
* `spectra` wire data/spectra-em.lut, the spectral upsampling table for emission

## parameters

* `film` the film id in the datafile
* `ev film` exposure correction when exposing the film
* `g film` gamma correction for exposing the film, use to adjust dynamic range
* `paper` the paper id in the datafile
* `ev paper` exposure correction when sensitising the paper
* `g paper` gamma correction when sensitising the paper, affects dynamic range and contrast
* `grain` switch grain simulation on or off
* `size` scale the grain size
* `uniform` uniformity of the grains as seen through the pixels. 1.0 means no variation at all
* `enlarger` mode of exposing the paper: output straight film negative or resize output. careful with 4x, it requires a lot of memory!
* `filter m` when exposing the print paper, dial in this share of magenta filter
* `filter y` when exposing the print paper, dial in this share of yellow filter
* `tune m` fine tune the magenta filter. think of this as a red/green tint
* `tune y` fine tune the yellow filter. think of this as a warm/cold white balance temperature
* `couplers` amount of developer inhibitor release couplers (affects colourfulness and local contrast)
