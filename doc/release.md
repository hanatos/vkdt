# release procedure

this document lists relevant steps in the release procedure.
action items are labelled (a)..(i).

## release branch

use semantic versioning and make a branch `release-0.6` to
contain tags for point releases `0.6.0` and `0.6.1` etc.
the `master` branch will carry on with new features and eventually become the
next release branch.

(a) remove useless/half broken/unfinished/stuff we don't want to maintain like
this in the future: simply delete module directory

(b) update release notes/changelog, and commit

## submodules

will be exported by `make release`.

`ext/imgui` is relatively recent

`ext/rawspeed` is a merge with the cr3 support branch

## version.h

`src/core/version.h` depends on `.git/FETCH_HEAD` if present.

to generate it for local testing purposes without pushing the tag
to the public repository, (c)
```
git tag -s '0.6.0' -m "this is release 0.6.0"
git push dreggn 0.6.0
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
tar xvJf vkdt-0.6.0.tar.xz
cd vkdt-0.6.0/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## upload

(g) push to public: `git push origin 0.6.0 release-0.6`

(h) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-0.6.0.tar.xz`

(i) github release announcement

* changelog
* acks
* checksum + dsc
* sign the tag

### current changelog

github actions: build linux, codeql
code cleanup/security fixes
perf overlay
gui fixes and finetune things
clean shutdown (write config file)
graph editor/delete graph edit tab in darkroom
initial focusstack blend mode
new autocrop button
preset for run-time gui noise profiling
frame-limiter for power saving
module graph editor coordinates stored in .cfg (see d7037435b21d420b0ca9b5abc0c1ee86abf29ab9)
libvkdt build option for 3rd party software, option to build cli only
dual screen support (display full image on second monitor)
zoomable mini-displays in right panel
key accels for rating/lables now configurable
custom font support for non ascii characters


## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 0.6.9999 -m "this is the beginning of the unreleased development branch which will become 0.7.0 eventually"
```
