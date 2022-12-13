# release procedure

this document lists relevant steps in the release procedure.

## release branch

use semantic versioning and make a branch `release-0.5` to
contain tags for point releases `0.5.0` and `0.5.1` etc.
the `master` branch will carry on with new features and eventually become the
next release branch.

## submodules

will be exported by `make release`.

`ext/imgui` is relatively recent

`ext/rawspeed` is a merge with the cr3 support branch

## version.h

`src/core/version.h` depends on `.git/FETCH_HEAD` if present.

to generate it for local testing purposes without pushing the tag
to the public repository,
```
git tag -a '0.5.0'
git push dreggn 0.5.0
git fetch --all
```

and for paranoia:

```
make src/core/version.h
cat src/core/version.h
```

## create tarball

`make release` and test in `/tmp`

## upload

* changelog
* acks
* checksum
* sign the tag

# changelog

this is the first release of `vkdt`. the project has been under development for
quite some years and so this is by no means a 'release early' kind of release.

since there is no previous version, here are a few noteworthy features of this release:

* very fast processing (gui and export use equivalent code path, no preview)
* video support (both ldr `.mov` and raw `.mlv`)
* timelapse support
* keyframe support for majority of parameters
* 3d rendering (quake!)
* support spectral input profiles
* drawn masks with pentablet support
* flexible metadata display
* script to create css/html only web gallery
* cr3 support via rawspeed
* multi-frame image alignment
