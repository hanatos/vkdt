# how to load custom fonts into vkdt

vkdt uses multi-signed distance fields for scale invariant font rendering.

these can be created by the full featured and matured `msdf-atlas-gen` utility.
this might be interesting for a customised look or if you depend on non-ascii
glyphs such as japanese codepoints.

to generate the required `.lut` files for vkdt, do the following:

build msdf-atlas-gen without the agonizing bloat or micr0soft tracking:
```
git clone --recursive https://github.com/Chlumsky/msdf-atlas-gen
cd msdf-atlas-gen
# the following two are not needed if you cloned recursively:
git submodule init
git submodule update -f
mkdir build
cd build
cmake -DMSDF_ATLAS_USE_VCPKG=off -DMSDF_ATLAS_USE_SKIA=off ..
make -j20
```

copy a ttf/otf of your choice (the example uses Roboto-Regular.ttf) into the `build/` directory, as well as
MaterialIcons-Regular.ttf for the ui. then generate the font atlas
```
bin/msdf-atlas-gen -outerpxpadding 2 -size 64 -yorigin top -type msdf -format png -imageout atlas.png -json metrics.json -font Roboto-Regular.ttf -chars "[' ','~']" -fontscale 1 -and -font MaterialIcons-Regular.ttf -chars 0xe01f,0xe020,0xe034,0xe037,0xe042,0xe044,0xe045,0xe047,0xe15b,0xe5e0,0xe5e1,0xe612,0xe836,0xe838 -fontscale 1
```
replace the `-chars` entries by your custom codepoint ranges. for instance for
chinese characters you can use `-chars [0x4e00,0x9fd0]` in conjunction with a font file
that holds these codepoints:
```
bin/msdf-atlas-gen -outerpxpadding 2 -size 64 -yorigin top -type msdf -format png -imageout atlas.png -json metrics.json -font 'NotoSansSC-Regular.ttf' -chars "[' ','~'],[0x4E00,0x9fd0]" -fontscale 1 -and -font MaterialIcons-Regular.ttf -chars 0xe01f,0xe020,0xe034,0xe037,0xe042,0xe044,0xe045,0xe047,0xe15b,0xe5e0,0xe5e1,0xe612,0xe836,0xe838 -fontscale 1
```
note that this can take a few minutes.

the material icon codepoints are needed for vkdt's ui rendering:
```
 0xe01f, // next keyframe
 0xe020, // prev keyframe
 0xe034, // pause
 0xe037, // play
 0xe042, // rewind
 0xe044, // next frame
 0xe045, // prev frame
 0xe047, // stop
 0xe15b, // module cannot be disabled
 0xe5e1, // expander closed
 0xe5e0, // expander open
 0xe612, // module disabled
 0xe836, // module not disabled
 0xe838, // star
```

then convert the png atlas to `.lut` by executing [fontlut.py](fontlut.py)
in this directory:
```
python fontlut.py
```
the two `.lut` files go into `bin/data` or your home directory `.config/vkdt/data`.

you can hold multiple font atlases in your data directory at the same time
and select which by pointing vkdt to one of them in the config file
`~/.config/vkdt/config.rc`:
```
..
strgui/msdffont:font
..
```
this entry above will use `font_metrics.lut` and `font_msdf.lut` (note the common
prefix of the file names).

if you want to install a chinese font at the side, inside the atlas gen build
directory, do
```
$ mv font_metrics.lut ~/.config/vkdt/data/cnfont_metrics.lut
$ mv font_msdf.lut ~/.config/vkdt/data/cnfont_msdf.lut
```
and set, in `~/.config/vkdt/config.rc`:
```
strgui/msdffont:cnfont
```
