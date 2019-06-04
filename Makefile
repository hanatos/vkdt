.PHONY:all ext src clean distclean bin

OPT_CFLAGS=-Wall -O3 -march=native -g
OPT_LDFLAGS=
CC=clang
CXX=clang++
GLSLC=glslangValidator
AR=ar
# dr dobb's idea about makefile debugging:
OLD_SHELL := $(SHELL)
# SHELL = $(warning [$@ ($^) ($?)])$(OLD_SHELL)
SHELL = $(warning [$@ ($?)])$(OLD_SHELL)
export OPT_CFLAGS OPT_LDFLAGS CC CXX GLSLC AR OLD_SHELL SHELL

all: ext src bin

# overwrites the above optimised build flags:
debug:OPT_CFLAGS+=-g -gdwarf-2 -ggdb3 -O0
debug:all

sanitize:OPT_CFLAGS=-fno-omit-frame-pointer -fsanitize=address -g
sanitize:OPT_LDFLAGS=-fsanitize=address
sanitize:all


ext: Makefile
	mkdir -p built/
	make -C ext/

src: ext Makefile
	mkdir -p built/
	make -C src/

clean:
	make -C ext/ clean
	make -C src/ clean

distclean:
	rm -rf built/
	rm -rf bin/

bin: src Makefile
	mkdir -p bin/data
	ln -sf ../src/cli/vkdt-cli bin/
	ln -sf ../src/gui/vkdt bin/
	ln -sf ../src/pipe/modules bin/
	cp ext/rawspeed/data/cameras.xml bin/data
