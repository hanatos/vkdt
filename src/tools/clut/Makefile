CC=clang
CFLAGS=-I../.. -I../shared -D_XOPEN_SOURCE=600 -Wall -std=c11 -g  -D_POSIX_C_SOURCE=200809L
CFLAGS+=-march=native -O2
# CFLAGS+=-fsanitize=address
# LDFLAGS+=-fsanitize=address
LDFLAGS=-lm

# CFLAGS+=-fno-omit-frame-pointer -fsanitize=address
# LDFLAGS+=-fsanitize=address

all: mkssf mkclut

DEPS=../../core/inpaint.h \
     ../../core/solve.h \
     ../../core/clip.h

spectra.lut: ../../../bin/data/spectra.lut
	ln -f ../../../bin/data/spectra.lut spectra.lut

mkclut: src/mkclut.c src/q2t.h ${DEPS} Makefile spectra.lut
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

SSFDEPS=src/dngproc.h\
        ../shared/cie1931.h\
        ../shared/matrices.h\
        src/cfa.h\
        src/cfa_data.h\
        src/cfa-plain.h\
        src/cfa-sigmoid.h\
        src/cfa-gauss.h

mkssf: src/mkssf.c ${SSFDEPS} Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

install: mkssf mkclut
	cp -f mkssf ../../../bin/vkdt-mkssf
	cp -f mkclut ../../../bin/vkdt-mkclut

clean:
	rm -rf mkclut mkssf
