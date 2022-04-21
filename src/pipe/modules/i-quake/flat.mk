QUAKE_I=pipe/modules/i-quake/quakespasm/Quake
QUAKE_L=pipe/modules/i-quake/quakespasm/Quake
MOD_C=pipe/connector.c
MOD_CFLAGS=-I$(QUAKE_I)
MOD_LDFLAGS=-L$(QUAKE_L) -lquakespasm -lm
MOD_LDFLAGS+=$(shell pkg-config --libs vorbis vorbisfile ogg mad)
pipe/modules/i-quake/libi-quake.so:pipe/modules/i-quake/quakespasm/Quake/libquakespasm.a
pipe/modules/i-quake/quakespasm/Quake/libquakespasm.a: pipe/modules/i-quake/quakespasm/Quake/Makefile
	make -C pipe/modules/i-quake/quakespasm/Quake
pipe/modules/i-quake/quakespasm/Quake/Makefile:
	$(shell git clone https://github.com/hanatos/quakespasm --depth 1 --branch master --single-branch pipe/modules/i-quake/quakespasm)
