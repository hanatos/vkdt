# release procedure

this document lists relevant steps in the release procedure.
action items are labelled (a)..(i).

## release branch

use semantic versioning and make a branch `release-1.0` to
contain tags for point releases `1.0.0` and `0.0.1` etc.
the `master` branch will carry on with new features and eventually become the
next release branch.

(a) remove useless/half broken/unfinished/stuff we don't want to maintain like
this in the future: simply delete module directory

(b) update release notes/changelog, and commit

## sub-projects

will be exported by `make release`, except the rust part which will be downloaded
during the build process (Cargo.lock takes care the version will be the same).

rawspeed, mcraw, and quakespasm will only be deployed depending on build
settings in `bin/config.mk` when issuing `make release`.

## version.h

`src/core/version.h` depends on `.git/FETCH_HEAD` if present.

to generate it for local testing purposes without pushing the tag
to the public repository, (c)
```
git tag -s '1.0.1' -m "this is release 1.0.1"
git push dreggn 1.0.1
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
tar xvJf vkdt-1.0.1.tar.xz
cd vkdt-1.0.1/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## create appimage

run `bin/mkappimg.sh` from the root directory
or push master same as release branch and grab
packages from github nightly ci.

## upload

(g) push to public: `git push origin 1.0.1 release-1.0`

(h) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-1.0.1.tar.xz`

(i) github release announcement

* changelog
* acks
* checksum + dsc
* sign the tag

### current changelog

new modules:
* `oidn` open image denoise
* `reinhard` tonemapper (phcreery)
* `hdrmerge` extend dynamic range (phcreery)
* `overlay` render text and latency data
* `opendrt` full-featured display transform

removed modules:
* `resnet` gmic neural denoiser (weights out of sync with upstream)

major features:
* `jddcnn` now for bayer and xtrans, with reconstruction for clean and noisy input
* drag keys and chord menu (randalix)
* revamped grade module with new colour wheel widget (randalix)
* denox codegen cnn modules, much faster per-arch spirv
* auto-growing thumbnail cache
* support for a variety of input colour spaces (pana-v, aces, ..)

graph internals:
* large memory support (work around some 32-bit limits of bindings in spirv)
* async compute support (niklasiivari)
* bind ssbo as buffer address references too
* `modify` type connectors
* resizing/resampling both sharper and smoother whenever possible (instrumental for cnn training)
* rewrote graph internals for buffer extent negotiations and allocation
* remove geometry shaders (for metal compat)
* centralise colour matrices, shared between cpu/gpu

smaller:
* low-lag present mode (phcreery)
* game controller fixes
* external resource tracking in the backend (colour luts, multiple input files, etc)
* `filmcurv` agx colour mode (phcreery)
* `filmcurv` luminance ratio colour mode (randalix)
* `inpaint` allow horizontal or vertical inpainting (keep horizons straight)
* `filmsim` expose halation radius
* `filmsim` coupler diffusion radius exposed
* `filmsim` like 2.5x faster (niklasiivari)
* `filmsim` with preflash (niklasiivari)

rendering:
* geometry nodes `g-ocean` `g-wobble` `g-csolid`
* use buffer address references for geo


## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 1.0.99 -m "this is the beginning of the unreleased development branch which will become 1.1.0 eventually"
```
