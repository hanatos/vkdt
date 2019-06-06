MODULES=$(wildcard modules/*/Makefile)
SPV=$(shell ls -1 modules/*/*.comp | sed -e 's/.comp/.spv/')
.PHONY: clean all modules $(MODULES)

CFLAGS+=-g
CFLAGS+=$(OPT_CFLAGS)

all: modules

modules: Makefile $(MODULES) $(SPV)

modules/*/Makefile:
	make -C $(dir $@)

%.spv: %.comp
	$(GLSLC) --target-env vulkan1.1 \
    -DQVK_SHADER \
    -V $< -o $@

clean:
	rm -f modules/*/lib*.so
