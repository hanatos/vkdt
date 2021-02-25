# entry point makefile.
# dispatches external builds and calls our main makefile in src.
# also handles some global settings for compilers and debug flags.

.PHONY:all ext src clean distclean bin

# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL RAWSPEED_PACKAGE_BUILD

all: ext src bin

# overwrites the above optimised build flags:
debug:OPT_CFLAGS+=-g -gdwarf-2 -ggdb3 -O0 -DQVK_ENABLE_VALIDATION
debug:OPT_LDFLAGS=
debug:all

sanitize:OPT_CFLAGS=-fno-omit-frame-pointer -fsanitize=address -g -O0
sanitize:OPT_LDFLAGS=-fsanitize=address
sanitize:all


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
	rm -rf built/
	rm -rf bin/

bin: src Makefile
	mkdir -p bin/data
	# copy so we can take the executable path via /proc/self/exe:
	cp -f src/vkdt-cli bin/
	cp -f src/vkdt bin/
	# should probably copy this for easier install, too:
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data
