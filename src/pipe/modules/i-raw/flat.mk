RAWSPEED_I=../ext/rawspeed
RAWSPEED_L=../built/ext/rawspeed
MOD_CFLAGS=-std=c++17 -Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/ $(shell pkg-config --cflags pugixml libjpeg)
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lz $(shell pkg-config --libs pugixml libjpeg)
ifeq ($(CXX),clang++)
MOD_LDFLAGS+=-lomp
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif
ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(shell pkg-config --cflags exiv2) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(shell pkg-config --libs exiv2)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
