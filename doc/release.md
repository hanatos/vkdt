# release procedure

this document lists relevant steps in the release procedure.
action items are labelled (a)..(h).

## release branch

use semantic versioning and make a branch `release-0.6` to
contain tags for point releases `0.6.0` and `0.6.1` etc.
the `master` branch will carry on with new features and eventually become the
next release branch.

(a) remove useless/half broken/unfinished/stuff we don't want to maintain like
this in the future: simply delete module directory

## submodules

will be exported by `make release`.

`ext/imgui` is relatively recent

`ext/rawspeed` is a merge with the cr3 support branch

## version.h

`src/core/version.h` depends on `.git/FETCH_HEAD` if present.

to generate it for local testing purposes without pushing the tag
to the public repository, (b)
```
git tag -s '0.6.0' -m "this is release 0.6.0"
git push dreggn 0.6.0
git fetch --all --tags
```

and for paranoia (c):

```
touch .git/FETCH_HEAD
make src/core/version.h
cat src/core/version.h
```

## create tarball

(d) `make release` and (e) test in `/tmp`:

```
cd /tmp
tar xvJf vkdt-0.6.0.tar.xz
cd vkdt-0.6.0/
make -j20
DESTDIR=/tmp/testrel make install
/tmp/testrel/usr/bin/vkdt
```

## upload

(e) push to public: `git push origin 0.6.0 release-0.6`

(g) sign the tarball:
`gpg -u jo@dreggn.org --detach-sign vkdt-0.6.0.tar.xz`

(h) github release announcement

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
git tag -s 0.6.9999 -m "this is the beginning of the unreleased development branch which will become 0.7.0 eventually"
```
