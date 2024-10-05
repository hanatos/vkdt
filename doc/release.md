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

## diverge branches

on the master branch, delete the changelog (to be filled with new features which
will also only be pushed to this branch). the release branch will be used for
bugfix/pointreleases.
tag the master/development branch as such, so dev packages will be ordered correctly:
```
git tag -s 1.0.99 -m "this is the beginning of the unreleased development branch which will become 1.1.0 eventually"
```
