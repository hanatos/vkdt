CC=clang
CFLAGS=-I../.. -D_XOPEN_SOURCE=600 -Wall -std=c11 -g  -D_POSIX_C_SOURCE=200809L
CFLAGS+=-march=native -O2
# LDFLAGS=-lm -llapack -lblas -lf2c -Llevmar-2.6/ -llevmar
# LDFLAGS=-lm -Llevmar-2.6/ -llevmar
LDFLAGS=-lm

# CFLAGS+=-fno-omit-frame-pointer -fsanitize=address
# LDFLAGS+=-fsanitize=address

all: vkdt-mkidt

DEPS=../../core/inpaint.h \
     ../../core/solve.h \
     ../../core/clip.h

vkdt-mkidt: main.c q2t.h ${DEPS} Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

install: vkdt-mkidt
	cp vkdt-mkidt ../../../bin/

clean:
	rm -rf vkdt-mkidt
