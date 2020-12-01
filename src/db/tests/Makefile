CC=clang
CFLAGS+=-Wall -std=c11 -D_GNU_SOURCE
CFLAGS+=-I../../core
#CFLAGS+=-march=native -O3
# CFLAGS+=-g -ggdb3 -fsanitize=address -fno-omit-frame-pointer
CFLAGS+=-g -ggdb3
#LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer

DEPS=../../core/core.h\
     ../../core/threads.h\
     ../../core/threads.c

test: test.c qsort.c qsort.h $(DEPS) Makefile
	$(CC) $(CFLAGS) $< qsort.c ../../core/threads.c -o test -lm -pthread $(LDFLAGS)

rtest: rtest.c sort.h $(DEPS) Makefile
	$(CC) $(CFLAGS) $< ../../core/threads.c -o rtest -lm -pthread $(LDFLAGS)

rc: rc.c ../rc.h ../stringpool.h ../murmur3.h ../db.h Makefile
	$(CC) $(CFLAGS) $< -I.. -o rc -lm $(LDFLAGS)
