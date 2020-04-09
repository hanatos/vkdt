RAWSPEED_I=../ext/rawspeed
RAWSPEED_L=../built/ext/rawspeed
MOD_CFLAGS=-Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lpugixml -lz -ljpeg
ifeq ($(CXX),clang++)
MOD_LDFLAGS+=-lomp
endif
ifeq ($(CXX),g++)
MOD_LDFLAGS+=-lgomp
endif
MOD_DEPS=$(RAWSPEED_L)/librawspeed.a
ifeq ($(VKDT_USE_EXIV2),1)
MOD_CFLAGS+=$(shell pkg-config --cflags exiv2) -DVKDT_USE_EXIV2=1
MOD_LDFLAGS+=$(shell pkg-config --libs exiv2)
pipe/modules/i-raw/libi-raw.so:pipe/modules/i-raw/exif.h
endif
ifeq ($(VKDT_USE_RAWSPEED),1)
MOD_BUILD=0
endif
