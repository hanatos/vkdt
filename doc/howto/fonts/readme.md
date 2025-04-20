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

copy a ttf/otf of your choice into the `build/` directory, as well as
MaterialIcons-Regular.ttf for the ui. then generate the font atlas
```
bin/msdf-atlas-gen -size 64 -yorigin top -type msdf -format png -imageout atlas.png -csv metrics.csv -font Roboto-Regular.ttf -chars "[' ','~']" -fontscale 1 -and -font MaterialIcons-Regular.ttf -chars 0xe01f,0xe020,0xe034,0xe037,0xe042,0xe044,0xe045,0xe047,0xe15b,0xe5c5,0xe5df,0xe612,0xe836,0xe838 -fontscale 1
```
replace the `-chars` entries by your custom codepoint ranges. the material icon
codepoints are needed for vkdt's ui rendering:
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
 0xe5c5, // expander closed
 0xe5df, // expander open
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
