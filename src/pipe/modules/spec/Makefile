CXX=clang++
CC=clang
CXXFLAGS=-msse2 -mfpmath=sse -g -Wall
CFLAGS=-g -Wall -std=c11
OPTFLAGS=-O3 -ffast-math -fno-finite-math-only -march=native

all: luts

macadam: macadam.c inpaint.h clip.h Makefile
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

mkspectra: createlut.c details/cie1931.h details/lu.h clip.h Makefile
	$(CC) $(CFLAGS) -DBAD_CMF=1 $(OPTFLAGS) -fopenmp $< -o $@ -lm

mkabney: createlut.c details/cie1931.h details/lu.h clip.h inpaint.h Makefile
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

luts: abney.lut spectra.lut

macadam.lut: macadam
	./macadam

spectra.lut: mkspectra macadam.lut
	./mkspectra 512 lut.pfm XYZ

abney.lut: mkabney macadam.lut
	./mkabney 2048 /dev/null XYZ

clean:
	rm -f mkspectra mkabney macadam
