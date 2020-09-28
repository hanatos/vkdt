CXX=clang++
CC=clang
CXXFLAGS=-msse2 -mfpmath=sse -g -Wall
OPTFLAGS=-O3 -ffast-math -fno-finite-math-only -march=native

all: rgb2spec_opt

rgb2spec_opt: rgb2spec_opt.cpp details/cie1931.h details/lu.h
	$(CXX) $(CXXFLAGS) $(OPTFLAGS) -std=c++11 -fopenmp $< -o $@ -lomp 

clean:
	rm -f rgb2spec_opt
