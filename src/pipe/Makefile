TARGET=libpipe.a
CFLAGS=-Wall -I. -I.. -fPIC
MODULES=$(wildcard modules/*/Makefile)
SPV=$(shell ls -1 modules/*/*.comp | sed -e 's/.comp/.spv/')
.PHONY: clean all debug modules $(MODULES)

CFLAGS+=-g
CFLAGS+=$(OPT_CFLAGS)

all: $(TARGET) modules

# now this also depends on vulkan headers in ../qvk
# put these dependencies here explicitly:
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

modules: Makefile $(MODULES) $(SPV)

modules/*/Makefile:
	make -C $(dir $@)

%.spv: %.comp
	$(GLSLC) --target-env vulkan1.1 \
    -DQVK_SHADER \
    -V $< -o $@


clean:
	rm -f $(TARGET)
	rm -f $(DEPS_O)
	rm -f modules/*/lib*.so
