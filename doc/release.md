# release procedure

this document lists relevant steps in the release procedure.
action items are labelled (a)..(i).

## release branch

use semantic versioning and make a branch `release-0.7` to
contain tags for point releases `0.7.0` and `0.7.1` etc.
the `master` branch will carry on with new features and eventually become the
next release branch.

(a) remove useless/half broken/unfinished/stuff we don't want to maintain like
this in the future: simply delete module directory

(b) update release notes/changelog, and commit

## submodules

will be exported by `make release`.

`ext/imgui` is relatively recent

rawspeed and quakespasm will only be deployed depending on build settings in `bin/config.mk`.

## version.h

`src/core/version.h` depends on `.git/FETCH_HEAD` if present.

to generate it for local testing purposes without pushing the tag
to the public repository, (c)
```
git tag -s '0.7.0' -m "this is release 0.7.0"
git push dreggn 0.7.0
git fetch --all --tags
```

and for paranoia (d):

```
touch .git/FETCH_HEAD
make src/core/version.h
cat src/core/version.h
```

## create tarball

(e) `make release` and (f) test in `/tmp`:

```
cd /tmp
tar xvJf vkdt-0.7.0.tar.xz
cd vkdt-0.7.0/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## create appimage

run `bin/mkappimg.sh` from the root directory

## upload

(g) push to public: `git push origin 0.7.0 release-0.7`

(h) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-0.7.0.tar.xz`

(i) github release announcement

* changelog
* acks
* checksum + dsc
* sign the tag

### current changelog

lots of small fixes for complicated/non-directed graphs, ui streamlining and polishing,
doc and website updates.

lighttable:

* add button to create video from timelapse sequence of raw files
* output modules all show their individual parameters now
* new automatic labels: video and bracket

darkroom:

* new dopesheet widget to manage keyframes
* animation controls
* introducing keyaccels: quickly bind small scripts to hotkeys (for instance
  expose up by half a stop, rotate ccw by one degree)

new modules:

* `autoexp` new module for autoexposure, most useful in video sequences
* `i-exr` initial openexr file support for reading
* new module `hotpx` to remove stuck pixels in bayer images

modules:

* `hilite` smooth out c/a artifacts at boundaries of overexposed regions
* `o-ffmpeg` support prores PQ/smpte 2084 10-bit output
* `i-vid` support 420, 422, and 8-, 10-, 12-, 16-bit input
* `quake` with more parameters and amd raytracing support
* `i-pfm` supports animation frames and noise profile arguments
* `pick` colours with freeze support for copy/pasting of settings
* `grade` with colour wheels input
* `blend` with option to invert mask
* `demosaic` now with RCD support. it's slower than the gaussian splats. some
  numbers for 16MP: gaussian splatting: 1.5ms, new RCD: 2.5ms, darktable's
  opencl implementation on the same image: 21ms.

build:

* `i-raw` update rawspeed to upstream + cr3 (but will not do it again)
* support rawler/rust backend to load raw images as replacement for rawspeed/exiv2
* initial .AppImage package provided


## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 0.7.9999 -m "this is the beginning of the unreleased development branch which will become 0.8.0 eventually"
```
