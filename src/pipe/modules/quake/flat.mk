QUAKE_I=pipe/modules/quake/quakespasm/Quake
QUAKE_L=pipe/modules/quake/quakespasm/Quake
MOD_C=pipe/connector.c
MOD_CFLAGS=-I$(QUAKE_I)
MOD_LDFLAGS=-L$(QUAKE_L) -lquakespasm -lm
MOD_LDFLAGS+=$(shell pkg-config --libs vorbis vorbisfile ogg mad)
pipe/modules/quake/libquake.so:pipe/modules/quake/quakespasm/Quake/libquakespasm.a
pipe/modules/quake/quakespasm/Quake/libquakespasm.a: pipe/modules/quake/quakespasm/Quake/Makefile
	make -C pipe/modules/quake/quakespasm/Quake
pipe/modules/quake/quakespasm/Quake/Makefile:
	$(shell git clone https://github.com/hanatos/quakespasm --depth 1 --branch master --single-branch pipe/modules/quake/quakespasm)
pipe/modules/quake/main.comp.spv:pipe/modules/quake/water.glsl
