TARGET=vkdt

.PHONY:clean

CFLAGS=-Wall -I. -I.. -g
LDFLAGS=-L../pipe -lpipe -ldl -lvulkan
CFLAGS+=$(OPT_CFLAGS) $(shell pkg-config --cflags sdl2)
LDFLAGS+=$(OPT_LDFLAGS) $(shell pkg-config --libs sdl2)

all:$(TARGET)

DEPS_H=../core/core.h\
       ../core/log.h
DEPS_C=../core/log.c\
       gui.c

DEPS_O=render.o

render.o:render.cpp Makefile
	$(CXX) -I ../../ext/imgui -I.. $(CXXFLAGS) $< -c -o $@

DEPS_A=../pipe/libpipe.a

$(TARGET):main.c $(DEPS_H) $(DEPS_C) $(DEPS_A) Makefile
	$(CC) $(CFLAGS) $(DEPS_C) main.c -o vkdt $(LDFLAGS) $(DEPS_O)

clean:
	rm -f $(TARGET)
