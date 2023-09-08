RAWSPEED_I=../ext/rawspeed
RAWSPEED_L=../built/ext/rawspeed
MOD_CFLAGS=-std=c++20 -Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ $(shell pkg-config --cflags pugixml libjpeg)
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lz $(shell pkg-config --libs pugixml libjpeg)
ifeq ($(CXX),clang++)
# omp has no pkg-config. this sucks so much:
OMP_LIB=$(shell grep OpenMP_omp_LIBRARY:FILEPATH ../built/ext/rawspeed/CMakeCache.txt | cut -f2 -d=)
MOD_LDFLAGS+=$(OMP_LIB)
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif
ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(shell pkg-config --cflags exiv2) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(shell pkg-config --libs exiv2)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
