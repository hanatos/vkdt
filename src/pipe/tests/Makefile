CC=clang
CFLAGS+=-O0 -Wall -I../.. -g
LDFLAGS+=

all: token alloc

token: token.c ../token.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

alloc: alloc.c ../alloc.h ../dlist.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)
