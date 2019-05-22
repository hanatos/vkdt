TARGET=libpipe.a
CC=clang
AR=ar
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

DEPS_O=alloc.o\
       connector.o\
       global.o\
       graph.o\
       module.o
DEPS_H=alloc.h\
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

%.o:%.c $(DEPS_H) Makefile
	$(CC) $(CFLAGS) $< -c -o $@

$(TARGET):$(DEPS_O) $(DEPS_H) Makefile
	$(AR) r $(TARGET) $(DEPS_O)

clean:
	rm -f $(TARGET)
	rm -f $(DEPS_O)
