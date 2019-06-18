CFLAGS+=-Wall -std=c11 -D_GNU_SOURCE
CFLAGS+=-I../../core -I ../../../ext/pthread-pool
CFLAGS+=-march=native -O3
#CFLAGS+=-g -ggdb3 -fsanitize=address -fno-omit-frame-pointer
#LDFLAGS+=-fsanitize=address -fno-omit-frame-pointer

DEPS=../../core/core.h\
     ../../core/threads.h\
     ../../core/threads.c
ADEPS=../../../ext/pthread-pool/libpthreadpool.a

test: test.c qsort.c qsort.h $(DEPS) Makefile
	$(CC) $(CFLAGS) $< qsort.c ../../core/threads.c $(ADEPS) -o test -lm -pthread $(LDFLAGS)

rtest: rtest.c sort.h $(DEPS) Makefile
	$(CC) $(CFLAGS) $< ../../core/threads.c $(ADEPS) -o rtest -lm -pthread $(LDFLAGS)
