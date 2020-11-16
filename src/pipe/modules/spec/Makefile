CXX=clang++
CC=clang
CXXFLAGS=-msse2 -mfpmath=sse -g -Wall
CFLAGS=-g -Wall
OPTFLAGS=-O3 -ffast-math -fno-finite-math-only -march=native

all: xy2sig rgb2mom

rgb2spec_opt: rgb2spec_opt.cpp details/cie1931.h details/lu.h
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -std=c++11 -fopenmp $< -o $@ -lomp 

rgb2mom: rgb2mom.c details/cie1931.h details/lu.h mom.h mbs3.h quant.h clip.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

xy2sig: xy2sig.c details/cie1931.h details/lu.h quant.h clip.h
	$(CC) $(CFLAGS) $(OPTFLAGS) -fopenmp $< -o $@ -lm

clean:
	rm -f rgb2spec_opt rgb2mom
