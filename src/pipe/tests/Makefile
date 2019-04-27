CC=clang
CFLAGS+=-O3 -Wall
LDFLAGS+=

all: token

token: token.c ../token.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
