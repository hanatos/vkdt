TARGET=libpipe.so
CC=clang
CFLAGS=-Wall -I. -I.. -fPIC
LDFLAGS=-ldl
.PHONY: clean all debug

CFLAGS+=-g
OPTFLAGS=-march=native -O3
CFLAGS+=$(OPTFLAGS)

all: $(TARGET)

# overwrites the above optimised build flags:
debug:CFLAGS+=-gdwarf-2 -ggdb3 -O0
debug:$(TARGET)

CFILES=alloc.c\
       connector.c\
       global.c\
       graph.c\
       module.c
HFILES=alloc.h\
       connector.h\
       dlist.h\
       global.h\
       graph.h\
       io.h\
       module.h\
       node.h\
       params.h\
       pipe.h\
       token.h

TARGET:$(CFILES) $(HFILES) Makefile
	$(CC) $(CFLAGS) $(CFILES) -shared -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)
