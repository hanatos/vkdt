CXX=clang++
CC=clang
CXXFLAGS=-msse2 -mfpmath=sse -g -Wall
CFLAGS=-g -Wall -std=c11
OPTFLAGS=-O3 -ffast-math -fno-finite-math-only -march=native

all: xy2sig rgb2mom

rgb2spec_opt: rgb2spec_opt.cpp details/cie1931.h details/lu.h
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -std=c++11 -fopenmp $< -o $@ -lomp 

rgb2mom: rgb2mom.c details/cie1931.h details/lu.h mom.h mbs3.h quant.h clip.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

xy2sig: xy2sig.c details/cie1931.h details/lu.h quant.h clip.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

mkspectra: createlut.c details/cie1931.h details/lu.h clip.h
	$(CC) $(CFLAGS) -DBAD_CMF=1 $(OPTFLAGS) -fopenmp $< -o $@ -lm

mkabney: createlut.c details/cie1931.h details/lu.h clip.h inpaint.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

luts: abney.lut spectra.lut

spectra.lut: mkspectra
	./mkspectra 512 lut.pfm XYZ

abney.lut: mkabney
	./mkabney 2048 /dev/null XYZ

clean:
	rm -f rgb2spec_opt rgb2mom mkspectra mkabney
