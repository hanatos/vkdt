CC=clang
CFLAGS+=-O0 -Wall -I../.. -g
LDFLAGS+=
CFLAGS+=-fno-omit-frame-pointer -fsanitize=address
LDFLAGS+=-fsanitize=address

all: token alloc pipe

token: token.c ../token.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

alloc: alloc.c ../alloc.h ../dlist.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

pipe: pipe.c ../global.h ../global.c ../token.h ../io.h ../connector.h ../connector.inc Makefile
	$(CC) $(CFLAGS) $< ../global.c -o $@ $(LDFLAGS) -ldl
