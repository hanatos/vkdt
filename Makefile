# for debugging, don't call into this directly, use the makefile in bin/ instead.
# you can, however, call 'make install' here to install to `prefix`.
# dispatches external builds and calls our main makefile in src.
# also handles some global settings for compilers and debug flags.

.PHONY:all ext src clean distclean bin install release cli
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
VKDTLIBDIR=/usr/local/lib/vkdt
VKDTINCDIR=/usr/local/include/vkdt
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
	cp -rfL bin/vkdt-noise-profile bin/vkdt-gallery bin/vkdt-read-icc ${VKDTDIR}
	ln -rsf ${VKDTDIR}/vkdt $(DESTDIR)$(prefix)/bin/vkdt
	ln -rsf ${VKDTDIR}/vkdt-cli $(DESTDIR)$(prefix)/bin/vkdt-cli

libinstall: ext lib bin
	mkdir -p $(VKDTLIBDIR)
	mkdir -p $(VKDTLIBDIR)/modules
	mkdir -p $(VKDTLIBDIR)/data
	mkdir -p $(VKDTINCDIR)
	mkdir -p $(VKDTINCDIR)/qvk
	mkdir -p $(VKDTINCDIR)/pipe
	mkdir -p $(VKDTINCDIR)/pipe/modules
	mkdir -p $(VKDTINCDIR)/core
	mkdir -p $(VKDTINCDIR)/gui
	cp -rfL bin/data ${VKDTLIBDIR}
	rsync -avP --include='**/params' --include='**/connectors' --include='**/*.ui' --include='**/ptooltips' --include='**/ctooltips' --include='**/readme.md' --include='**.spv' --include='**.so' --include '*/' --exclude='**' bin/modules/ ${VKDTLIBDIR}/modules/
	cp -rfL bin/libvkdt.so ${VKDTLIBDIR} 
	cp -rfL bin/libvkdt.h $(VKDTINCDIR)
	cp -rfL src/qvk/*.h $(VKDTINCDIR)/qvk
	cp -rfL src/pipe/*.h $(VKDTINCDIR)/pipe
	cp -rfL src/pipe/modules/*.h $(VKDTINCDIR)/pipe/modules
	cp -rfL src/gui/*.h $(VKDTINCDIR)/gui
	cp -rfL src/core/*.h $(VKDTINCDIR)/core
	echo "$(VKDTLIBDIR)" > /etc/ld.so.conf.d/libvkdt.conf
	ldconfig

VERSION=$(shell grep VERSION src/core/version.h | cut -d'"' -f2)
release: Makefile src/core/version.h
	@echo packing up version ${VERSION}
	$(shell (echo src/core/version.h; git ls-files --recurse-submodules) | tar caf vkdt-${VERSION}.tar.xz --xform s:^:vkdt-${VERSION}/: --verbatim-files-from -T-)

# overwrites the above optimised build flags:
debug:OPT_CFLAGS=-g -gdwarf-2 -ggdb3 -O0 -DQVK_ENABLE_VALIDATION -DDEBUG_MARKERS
# debug:OPT_CFLAGS=-g -gdwarf-2 -ggdb3 -O0
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

reload-shaders: Makefile
	$(MAKE) -C src/ reload-shaders

CLI=../bin/vkdt-cli ../bin/vkdt-fit
cli: Makefile bin ext
	$(MAKE) -C src/ ${CLI} tools modules

LIB=../bin/libvkdt.so
lib: Makefile bin ext
	$(MAKE) -C src/ ${LIB} modules
	cp -p src/lib/libvkdt.h bin/

clean:
	$(MAKE) -C ext/ clean
	$(MAKE) -C src/ clean

distclean:
	$(shell find . -name "*.o"   -exec rm {} \;)
	$(shell find . -name "*.spv" -exec rm {} \;)
	$(shell find . -name "*.so"  -exec rm {} \;)
	rm -rf bin/vkdt bin/vkdt-fit bin/vkdt-cli bin/vkdt-mkssf bin/vkdt-mkclut bin/vkdt-lutinfo bin/vkdt-eval-profile bin/libvkdt.so
	rm -rf src/macadam
	rm -rf src/mkabney
	rm -rf bin/data/*.lut
	rm -rf bin/data/cameras.xml
	rm -rf built/
	rm -rf bin/modules
	rm -rf src/macadam.lut

libclean:
	rm -rf $(VKDTLIBDIR)
	rm -rf $(VKDTINCDIR)
	rm -r /etc/ld.so.conf.d/libvkdt.conf
	ldconfig

bin: Makefile
	mkdir -p bin/data
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data

