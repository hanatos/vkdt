# for debugging, don't call into this directly, use the makefile in bin/ instead.
# you can, however, call 'make install' here to install to `prefix`.
# dispatches external builds and calls our main makefile in src.
# also handles some global settings for compilers and debug flags.

.PHONY:all ext src clean distclean bin install release
include bin/config.mk.defaults
sinclude bin/config.mk

# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL RAWSPEED_PACKAGE_BUILD

all: ext src bin

prefix?=/usr
DESTDIR?=
VKDTDIR?=$(DESTDIR)$(prefix)/lib/vkdt
install: all
	mkdir -p $(VKDTDIR)
	mkdir -p $(VKDTDIR)/modules
	mkdir -p $(DESTDIR)$(prefix)/bin
	cp -rfL bin/data ${VKDTDIR}
	rsync -avP --include='**/params' --include='**/connectors' --include='**/*.ui' --include='**/ptooltips' --include='**/ctooltips' --include='**/readme.md' --include='**.spv' --include='**.so' --include '*/' --exclude='**' bin/modules/ ${VKDTDIR}/modules/
	cp -rfL bin/vkdt ${VKDTDIR}
	cp -rfL bin/vkdt-cli ${VKDTDIR}
	cp -rfL bin/vkdt-mkssf bin/vkdt-mkclut bin/vkdt-fit ${VKDTDIR}
	cp -rfL bin/vkdt-eval-profile bin/vkdt-lutinfo ${VKDTDIR}
	cp -rfL bin/default* ${VKDTDIR}
	cp -rfL bin/darkroom.ui ${VKDTDIR}
	cp -rfL bin/thumb.cfg ${VKDTDIR}
	cp -rfL bin/vkdt-noise-profile bin/vkdt-gallery bin/vkdt-read-icc ${VKDTDIR}
	ln -rsf ${VKDTDIR}/vkdt $(DESTDIR)$(prefix)/bin/vkdt
	ln -rsf ${VKDTDIR}/vkdt-cli $(DESTDIR)$(prefix)/bin/vkdt-cli

VERSION=$(shell grep VERSION src/core/version.h | cut -d'"' -f2)
release: Makefile src/core/version.h
	@echo packing up version ${VERSION}
	$(shell (echo src/core/version.h; git ls-files --recurse-submodules) | tar caf vkdt-${VERSION}.tar.xz --xform s:^:vkdt-${VERSION}/: --verbatim-files-from -T-)

# overwrites the above optimised build flags:
# debug:OPT_CFLAGS=-g -gdwarf-2 -ggdb3 -O0 -DQVK_ENABLE_VALIDATION -DDEBUG_MARKERS
debug:OPT_CFLAGS=-g -gdwarf-2 -ggdb3 -O0
debug:OPT_LDFLAGS=
debug:all

sanitize:OPT_CFLAGS=-fno-omit-frame-pointer -fsanitize=address -g -O0
sanitize:OPT_LDFLAGS=-fsanitize=address
sanitize:all

sanitize-thread:OPT_CFLAGS=-fsanitize=thread -g -O0
sanitize-thread:OPT_LDFLAGS=-fsanitize=thread
sanitize-thread:all

ext: Makefile
	mkdir -p built/
	$(MAKE) -C ext/

src: ext Makefile
	mkdir -p built/
	$(MAKE) -C src/

clean:
	$(MAKE) -C ext/ clean
	$(MAKE) -C src/ clean

distclean:
	$(shell find . -name "*.o"   -exec rm {} \;)
	$(shell find . -name "*.spv" -exec rm {} \;)
	$(shell find . -name "*.so"  -exec rm {} \;)
	rm -rf bin/vkdt bin/vkdt-fit bin/vkdt-cli bin/vkdt-mkssf bin/vkdt-mkclut bin/vkdt-lutinfo bin/vkdt-eval-profile
	rm -rf src/macadam
	rm -rf src/mkabney
	rm -rf bin/data/*.lut
	rm -rf bin/data/cameras.xml
	rm -rf built/
	rm -rf bin/modules
	rm -rf src/macadam.lut

bin: src Makefile
	mkdir -p bin/data
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data

