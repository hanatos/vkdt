QUAKE_I=pipe/modules/quake/quakespasm/Quake
QUAKE_L=pipe/modules/quake/quakespasm/Quake
MOD_C=pipe/connector.c
MOD_CFLAGS=-I$(QUAKE_I) $(VKDT_QEXTRA_CFLAGS)
MOD_LDFLAGS=-L$(QUAKE_L) -lquakespasm -lm
MOD_LDFLAGS+=$(VKDT_QEXTRA_LDFLAGS)
pipe/modules/quake/libquake.so:pipe/modules/quake/quakespasm/Quake/libquakespasm.a
pipe/modules/quake/quakespasm/Quake/libquakespasm.a: pipe/modules/quake/quakespasm/Quake/Makefile
	make -C pipe/modules/quake/quakespasm/Quake
pipe/modules/quake/quakespasm/Quake/Makefile:
	$(shell git clone https://github.com/hanatos/quakespasm --depth 1 --branch master --single-branch pipe/modules/quake/quakespasm)
	rm -rf pipe/modules/quake/quakespasm/Linux
	rm -rf pipe/modules/quake/quakespasm/MacOSX
	rm -rf pipe/modules/quake/quakespasm/Misc
	rm -rf pipe/modules/quake/quakespasm/Windows
pipe/modules/quake/main.comp.spv:pipe/modules/quake/water.glsl
pipe/modules/quake/main.comp.spv:pipe/modules/shared/render3d.glsl
pipe/modules/quake/main.comp.spv:pipe/modules/quake/config.h
pipe/modules/quake/libquake.so:pipe/modules/quake/config.h
