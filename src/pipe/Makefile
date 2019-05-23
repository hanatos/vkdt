TARGET=libpipe.a
CC=clang
AR=ar
CFLAGS=-Wall -I. -I.. -fPIC
LDFLAGS=-ldl
MODULES=$(wildcard modules/*/Makefile)
.PHONY: clean all debug modules $(MODULES)

CFLAGS+=-g
OPTFLAGS=-march=native -O3
CFLAGS+=$(OPTFLAGS)

all: $(TARGET) modules

# overwrites the above optimised build flags:
debug:CFLAGS+=-gdwarf-2 -ggdb3 -O0
debug:$(TARGET)

# TODO: now this also depends on vulkan headers in ../qvk
# TODO: put these dependencies here explicitly:
DEPS_QVK=../qvk/%.h

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

%.o:%.c $(DEPS_H) $(DEPS_QVK) Makefile
	$(CC) $(CFLAGS) $< -c -o $@

$(TARGET):$(DEPS_O) Makefile
	$(AR) r $(TARGET) $(DEPS_O)

modules: Makefile $(MODULES)

modules/*/Makefile:
	make -C $(dir $@)

clean:
	rm -f $(TARGET)
	rm -f $(DEPS_O)
	rm -f modules/*/lib*.so
