# use rawspeed
ifeq ($(VKDT_USE_RAWINPUT),1)
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
# XXX stuff from ext/ previously
# XXX TODO:
# cp ext/rawspeed/data/cameras.xml bin/data
# .PHONY:all clean rawspeed
# BUILT=../built/ext
# 
# all: rawspeed
# 
# rawspeed: Makefile $(BUILT)/rawspeed/librawspeed.a
# 
# $(BUILT)/rawspeed/Makefile: Makefile
# ifneq ($(VKDT_USE_RAWSPEED),1)
#   mkdir -p $(BUILT)/rawspeed
#   touch $(BUILT)/rawspeed/Makefile
# else
# ifeq ($(RAWSPEED_PACKAGE_BUILD),0)
#   mkdir -p $(BUILT)/rawspeed
#   cd $(BUILT)/rawspeed; cmake ../../../ext/rawspeed -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off
# else
#   mkdir -p $(BUILT)/rawspeed
#   cd $(BUILT)/rawspeed; cmake ../../../ext/rawspeed -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off -DBINARY_PACKAGE_BUILD=on
# endif
# endif
# 
# # for some reason this is challenging for older? cmake
# # cmake -S rawspeed/ -B $(BUILT)/rawspeed/ -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release
# 
# $(BUILT)/rawspeed/librawspeed.a: Makefile $(BUILT)/rawspeed/Makefile
#   mkdir -p $(BUILT)/rawspeed
# ifneq ($(VKDT_USE_RAWSPEED),1)
#   touch $(BUILT)/rawspeed/librawspeed.a
# else
#   $(MAKE) -C $(BUILT)/rawspeed
#   strip -S $(BUILT)/rawspeed/librawspeed.a
# endif
# 
# clean: Makefile

# XXX for rawloader too?
ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(shell pkg-config --cflags exiv2) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(shell pkg-config --libs exiv2)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
endif

# TODO: something distclean to rm -rf the checkout?
# TODO: cache a hash to the checkout revision and test against what we want

# use rawloader
ifeq ($(VKDT_USE_RAWINPUT),2)
MOD_LDFLAGS=pipe/modules/i-raw/rawloader-c/target/release/librawloader.a
MOD_CFLAGS=-Ipipe/modules/i-raw/rawloader-c
pipe/modules/i-raw/libi-raw.so: pipe/modules/i-raw/rawloader-c/target/release/librawloader.a pipe/modules/i-raw/rawloader/target/release/librawloader.rlib

pipe/modules/i-raw/rawloader/Cargo.toml:
	$(shell git clone https://github.com/pedrocr/rawloader --depth 1 --branch master --single-branch pipe/modules/i-raw/rawloader)

pipe/modules/i-raw/rawloader-c/target/release/librawloader.a: pipe/modules/i-raw/rawloader/target/release/librawloader.rlib
	cd pipe/modules/i-raw/rawloader-c; cargo build --release

pipe/modules/i-raw/rawloader/target/release/librawloader.rlib: pipe/modules/i-raw/rawloader/Cargo.toml
	cd pipe/modules/i-raw/rawloader; cargo build --release
endif
