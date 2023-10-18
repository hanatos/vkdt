# use rawspeed
ifeq ($(VKDT_USE_RAWINPUT),1)
RAWSPEED_I=pipe/modules/i-raw/rawspeed
RAWSPEED_L=pipe/modules/i-raw/rawspeed/build
MOD_CFLAGS=-std=c++20 -Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ $(shell pkg-config --cflags pugixml libjpeg)
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lz $(shell pkg-config --libs pugixml libjpeg)

pipe/modules/i-raw/libi-raw.so: $(RAWSPEED_L)/librawspeed.a

# omp has no pkg-config. this sucks so much:
ifeq ($(CXX),clang++)
MOD_LDFLAGS+=-fopenmp=libomp
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif


$(RAWSPEED_L)/Makefile: $(RAWSPEED_I)/CMakeLists.txt
	mkdir -p $(RAWSPEED_L)
ifeq ($(RAWSPEED_PACKAGE_BUILD),0)
	cd $(RAWSPEED_L); cmake .. -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off
else
	cd $(RAWSPEED_L); cmake .. -G "Unix Makefiles" -DBUILD_TESTING=0 -DCMAKE_BUILD_TYPE=release -DRAWSPEED_ENABLE_LTO=off -DRAWSPEED_ENABLE_DEBUG_INFO=off -DBUILD_BENCHMARKING=off -DBUILD_FUZZERS=off -DBUILD_TOOLS=off -DBINARY_PACKAGE_BUILD=on
endif

$(RAWSPEED_L)/librawspeed.a: $(RAWSPEED_L)/Makefile
	cp $(RAWSPEED_I)/data/cameras.xml ../bin/data
	$(MAKE) -C $(RAWSPEED_L)
	strip -S $(RAWSPEED_L)/librawspeed.a

$(RAWSPEED_I)/CMakeLists.txt:
	$(shell git clone https://github.com/hanatos/rawspeed.git --depth 1 --branch rebase_cr3 --single-branch $(RAWSPEED_I))

ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(shell pkg-config --cflags exiv2) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(shell pkg-config --libs exiv2)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
endif # end rawspeed

# TODO: cache a hash to the checkout revision and test against what we want

# use rawloader
ifeq ($(VKDT_USE_RAWINPUT),2)
MOD_LDFLAGS=pipe/modules/i-raw/rawloader-c/target/release/librawloader.a
MOD_CFLAGS=-Ipipe/modules/i-raw/rawloader-c
pipe/modules/i-raw/libi-raw.so: pipe/modules/i-raw/rawloader-c/target/release/librawloader.a

pipe/modules/i-raw/rawloader-c/target/release/librawloader.a: pipe/modules/i-raw/rawloader-c/lib.rs
	cd pipe/modules/i-raw/rawloader-c; cargo build --release
endif
