TARGET=vkdt-cli

.PHONY:clean

CFLAGS=-Wall -I. -I.. -g
LDFLAGS=-L../pipe -lpipe -ldl -lvulkan
CFLAGS+=$(OPT_CFLAGS)
LDFLAGS+=$(OPT_LDFLAGS)

DEPS_H=../core/core.h\
       ../core/log.h
DEPS_C=../core/log.c
DEPS_A=../pipe/libpipe.a

$(TARGET):main.c $(DEPS_H) $(DEPS_C) $(DEPS_A) Makefile
	$(CC) $(CFLAGS) $(DEPS_C) main.c -o vkdt-cli $(LDFLAGS)

clean:
	rm -f $(TARGET)
