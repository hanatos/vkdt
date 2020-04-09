# entry point makefile.
# dispatches external builds and calls our main makefile in src.
# also handles some global settings for compilers and debug flags.

.PHONY:all ext src clean distclean bin

OPT_CFLAGS=-Wall -pipe -O3 -march=native -g
OPT_LDFLAGS=
GLSLC=glslangValidator
AR=ar
# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL

all: ext src bin

# overwrites the above optimised build flags:
debug:OPT_CFLAGS+=-g -gdwarf-2 -ggdb3 -O0 -DQVK_ENABLE_VALIDATION
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
	ln -sf ../src/vkdt-cli bin/
	ln -sf ../src/vkdt bin/
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data
