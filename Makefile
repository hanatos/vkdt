# for debugging, don't call into this directly, use the makefile in bin/ instead.
# you can, however, call 'make install' here to install to `prefix`.
# dispatches external builds and calls our main makefile in src.
# also handles some global settings for compilers and debug flags.

.PHONY:all src clean distclean bin install release cli
ifeq ($(OS),Windows_NT)
include bin/config.mk.defaults.w64
else
ifeq ($(shell uname),Darwin)
include bin/config.mk.defaults.osx
else
include bin/config.mk.defaults
endif
endif
sinclude bin/config.mk

# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL

all: src bin

prefix?=/usr
DESTDIR?=
ifeq ($(OS),Windows_NT)
VKDTDIR?=$(shell cygpath -u $(DESTDIR)$(prefix)/lib/vkdt)
else
VKDTDIR?=$(DESTDIR)$(prefix)/lib/vkdt
endif
VKDTLIBDIR?=$(DESTDIR)$(prefix)/lib
VKDTINCDIR?=$(DESTDIR)$(prefix)/include/vkdt
install-bin: all Makefile
	mkdir -p $(VKDTDIR)/lib
	mkdir -p $(DESTDIR)$(prefix)/bin
	ln -rsf ${VKDTDIR}/vkdt $(DESTDIR)$(prefix)/bin/vkdt || true
	ln -rsf ${VKDTDIR}/vkdt-cli $(DESTDIR)$(prefix)/bin/vkdt-cli || true
	cp -rfL bin/vkdt ${VKDTDIR}
	cp -rfL bin/vkdt-cli ${VKDTDIR}
	cp -rfL bin/exiftool ${VKDTDIR}
	cp -rfL bin/vkdt-mkssf bin/vkdt-mkclut bin/vkdt-fit ${VKDTDIR}
	cp -rfL bin/vkdt-eval-profile bin/vkdt-lutinfo ${VKDTDIR}
	cp -rfL bin/vkdt-noise-profile bin/vkdt-gallery bin/vkdt-read-icc ${VKDTDIR}
	cp -rfL bin/darkroom.ui bin/style.txt ${VKDTDIR}
ifneq ($(OS), Windows_NT)
ifeq ($(shell uname),Linux)
	cp -rfL vkdt.png $(DESTDIR)$(prefix)/share/icons/
	cp -rfL vkdt.desktop $(DESTDIR)$(prefix)/share/applications/
endif
endif

install-mod: bin Makefile
	mkdir -p $(VKDTDIR)/modules
	rsync -avP --include='**/params' --include='**/connectors' --include='**/*.ui' --include='**/ptooltips' --include='**/ctooltips' --include='**/readme.md' --include='**.spv' --include='**.so' --include '*/' --exclude='**' bin/modules/ ${VKDTDIR}/modules/
	cp -rfL bin/data ${VKDTDIR}
	cp -rfL bin/default* ${VKDTDIR}

install: install-bin install-mod Makefile

src/core/version.h: $(wildcard .git/FETCH_HEAD)
	@echo "#pragma once" > src/core/version.h
	@echo "#define VKDT_VERSION \"$(shell git describe)\"" >> src/core/version.h

install-lib: install-mod Makefile src/core/version.h
	mkdir -p $(VKDTINCDIR)/qvk
	mkdir -p $(VKDTINCDIR)/pipe
	mkdir -p $(VKDTINCDIR)/pipe/modules
	mkdir -p $(VKDTINCDIR)/core
	mkdir -p $(VKDTINCDIR)/gui
	cp -rfL bin/libvkdt.so ${VKDTLIBDIR} 
	cp -rfL src/lib/vkdt.h $(VKDTINCDIR)
	cp -rfL src/qvk/*.h $(VKDTINCDIR)/qvk
	cp -rfL src/pipe/*.h $(VKDTINCDIR)/pipe
	cp -rfL src/pipe/modules/*.h $(VKDTINCDIR)/pipe/modules
	cp -rfL src/gui/*.h $(VKDTINCDIR)/gui
	cp -rfL src/core/*.h $(VKDTINCDIR)/core

RELEASE_FILES=$(shell echo src/core/version.h; git ls-files --recurse-submodules)
ifeq ($(VKDT_USE_RAWINPUT), 1)
  RAWSPEED_DIR=$(shell ls -d src/pipe/modules/i-raw/rawspeed-*)
  RELEASE_FILES+=$(shell cd $(RAWSPEED_DIR) && git ls-files | sed -e 's\#^\#$(RAWSPEED_DIR)/\#')
endif
ifeq ($(VKDT_USE_MCRAW), 1)
  MCRAW_DIR=$(shell ls -d src/pipe/modules/i-mcraw/mcraw-*)
  RELEASE_FILES+=$(shell cd $(MCRAW_DIR) && git ls-files | sed -e 's\#^\#$(MCRAW_DIR)/\#')
endif
ifeq ($(VKDT_USE_QUAKE), 1)
  RELEASE_FILES+=$(shell cd src/pipe/modules/quake/quakespasm; git ls-files | sed -e 's\#^\#src/pipe/modules/quake/quakespasm/\#')
endif
RELEASE_FILES:=$(filter-out .%,$(RELEASE_FILES))

VERSION=$(shell grep VERSION src/core/version.h | cut -d'"' -f2)
release: Makefile src/core/version.h
	@echo packing up version ${VERSION}
	$(shell (echo ${RELEASE_FILES} | sed -e 's/ /\n/g' | tar caf vkdt-${VERSION}.tar.xz --xform s:^:vkdt-${VERSION}/: --verbatim-files-from -T-))

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

src: Makefile src/core/version.h
	$(MAKE) -C src/

reload-shaders: Makefile
	$(MAKE) -C src/ reload-shaders

CLI=../bin/vkdt-cli ../bin/vkdt-fit
cli: Makefile bin src/core/version.h
	$(MAKE) -C src/ $(CLI) tools modules

LIB=../bin/libvkdt.so
lib: Makefile bin src/core/version.h
	$(MAKE) -C src/ $(LIB) modules

clean:
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
	rm -rf bin/modules
	rm -rf src/macadam.lut
	$(MAKE) -C src distclean

uninstall-lib:
	rm -rf $(VKDTLIBDIR)/libvkdt.so $(VKDTLIBDIR)/modules  $(VKDTLIBDIR)/data
	rm -rf $(VKDTINCDIR)

bin: Makefile
	mkdir -p bin/data
ifeq ($(OS),Windows_NT)
	cp -rf src/pipe/modules bin/
else
	ln -sf ../src/pipe/modules bin/
endif
