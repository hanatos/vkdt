# release procedure

this document lists relevant steps in the release procedure.
action items are labelled (a)..(i).

## release branch

use semantic versioning and make a branch `release-0.8` to
contain tags for point releases `0.8.0` and `0.8.1` etc.
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
git tag -s '0.8.0' -m "this is release 0.8.0"
git push dreggn 0.8.0
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
tar xvJf vkdt-0.8.0.tar.xz
cd vkdt-0.8.0/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## create appimage

run `bin/mkappimg.sh` from the root directory

## upload

(g) push to public: `git push origin 0.8.0 release-0.8`

(h) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-0.8.0.tar.xz`

(i) github release announcement

* changelog
* acks
* checksum + dsc
* sign the tag

### current changelog

main features:
neural network infrastructure. better `loss` node with
logarithmic plotting. `kpn`: kernel predicting neural network for
denoising, training included as `kpn-t`. based on tiny mlp using
tensor cores/registers/shm.

new modules:
* `mask` create data-dependent masks from luminance, hue, or saturation of an image
* `mv2rot` compute rotation/translation from per pixel motion vectors
* `o-exr` write openexr images with chromaticities attribute
* `o-csv` dump image data to text file

legacy modules:
* `cnn`: renamed to `resnet` (training in gmic may be incompatible now)
* `menon`: superseded by rcd mode in demosaic module

platform compatibility:
* move platform dependent code to `fs.h` and implement some windows stuff

other:
* rawler/rust-c interface updated
* better video support

bugfixes and code cleanups, gui streamlining things.


## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 0.8.99 -m "this is the beginning of the unreleased development branch which will become 0.9.0 eventually"
```
