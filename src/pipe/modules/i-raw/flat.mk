RAWSPEED_I=../ext/rawspeed
RAWSPEED_L=../built/ext/rawspeed
MOD_CFLAGS=-Wall -I$(RAWSPEED_I)/src/librawspeed/ -I$(RAWSPEED_L)/src/ -I$(RAWSPEED_I)/src/external/
MOD_LDFLAGS=-L$(RAWSPEED_L) -lrawspeed -lpugixml -lomp -lz -ljpeg
MOD_DEPS=$(RAWSPEED_L)/librawspeed.a
