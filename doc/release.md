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
git tag -s '1.0.0' -m "this is release 1.0.0"
git push dreggn 1.0.0
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
tar xvJf vkdt-1.0.0.tar.xz
cd vkdt-1.0.0/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## create appimage

run `bin/mkappimg.sh` from the root directory
or push master same as release branch and grab
packages from github nightly ci.

## upload

(g) push to public: `git push origin 1.0.0 release-1.0`

(h) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-1.0.0.tar.xz`

(i) github release announcement

* changelog
* acks
* checksum + dsc
* sign the tag

### current changelog

#### big features:
* [spectral film simulation](../src/pipe/modules/filmsim/readme.md)  
  thanks to arctic for crunching all the data and doing the research!  
  thanks to jedypod for adding the latest film/paper stock!
* [scalable msdf fonts](./howto/fonts/readme.md) for sharp fonts in graph editor at any zoom level
* hdr support, if you use wayland and the compositor does it, try  
  `ENABLE_HDR_WSI=1 vdkt`  
  (see [display colour docs](../doc/howto/colour-display/readme.md) for details)
* quick access preset buttons in customisable favourites tab
* arch aur package `vkdt-git`
* rewrote `vkdt read-icc` in c to reduce dependencies for packaging
* filtered minification for display (improved grain/noise inspection)
* introduce [core and beta modules](./core.md) for backward compatibility guarantees
* rgb knobs / oklab widget (see `frame` or `grade` module for an example)
* android compile fixes (thanks paolo!)
* dynamic lod (option to render low res when dragging sliders/drawing masks)
* keyframe interpolation modes (ease-in, ease-out, ..)
* keyframed brush strokes (for real time hand drawn text for instance)
* vkdt-rendered logo (looks best in hdr mode)
* improved wayland support (better than x11)
* [i-lut](../src/pipe/modules/i-lut/readme.md) now supports lists of input files, useful for textures/3d rendering
* improved the [grain module](../src/pipe/modules/grain/readme.md) with additional parameters for low frequency components
* introduce mouse interaction on `dspy` outputs of modules to change their parameters. 
  the new rgb `curves` module is the first user.
* use [hsluv](https://www.hsluv.org/) as ui colour picker space
* re-introduced darkroom mode zoom/pan via gamepad
* refactored `quake` and `rt` modules to use a unified spectral renderer
* an implementation of [mcpg](https://www.lalber.org/2025/04/markov-chain-path-guiding/) (Markov chain path guiding) for 3d rendering/quake
* ui fixes and polish
* removed some old modules

#### new modules
* [dehaze](../src/pipe/modules/dehaze/readme.md) haze removal and depth estimator
* [rgb curves](../src/pipe/modules/curves/readme.md) simple per-channel/luminance curve widget
* [o-copy](../src/pipe/modules/o-copy/readme.md) to copy raw images on export
* [jddcnn](../src/pipe/modules/jddcnn/readme.md) experimental joint demosaicing and denoising based on a CNN
* [g-csolid](../src/pipe/modules/g-csolid/readme.md) convert drawn brush strokes to 3d geometry
* [bvh](../src/pipe/modules/bvh/readme.md) 3d geometry sink for ray tracing
* [i-obj](../src/pipe/modules/i-obj/readme.md) 3d geometry source
* [physarum](../src/pipe/modules/physarum/readme.md) mesmerizing slime mould particle sim


## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 1.0.99 -m "this is the beginning of the unreleased development branch which will become 1.1.0 eventually"
```
