CC=clang
CFLAGS+=-O0 -Wall -I../.. -g
LDFLAGS=-ldl -L../../qvk -lqvk -lvulkan
# doesn't play so well with the rawspeed module so far:
CFLAGS+=-fno-omit-frame-pointer -fsanitize=address
LDFLAGS+=-fsanitize=address

all: token alloc pipe graph

token: token.c ../token.h Makefile
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

alloc: alloc.c ../alloc.h ../alloc.c ../dlist.h Makefile
	$(CC) $(CFLAGS) $< ../alloc.c -o $@ $(LDFLAGS)

GRAPH_DEPS=../graph.h\
           ../graph-traverse.inc\
           ../alloc.h\
           ../connector.h\
           ../connector.inc\
           ../dlist.h\
           ../global.h\
           ../module.h\
           ../token.h
GRAPH_C= ../graph.c\
         ../alloc.c\
         ../connector.c\
         ../global.c\
         ../module.c\
         ../../core/log.c

pipe: pipe.c $(GRAPH_DEPS) Makefile
	$(CC) $(CFLAGS) $< $(GRAPH_C) -o $@ $(LDFLAGS)

graph: graph.c $(GRAPH_DEPS) $(GRAPH_C) Makefile
	$(CC) $(CFLAGS) $< $(GRAPH_C) -o $@ $(LDFLAGS)
