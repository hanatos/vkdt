# flat makefile for everything.

# modules which need a dso have a makefile, collect all these.
# also all .comp compute shaders will be compiled to spir-v.
MODULES=$(wildcard pipe/modules/*/Makefile)
SPV=$(shell ls -1 pipe/modules/*/*.comp | sed -e 's/.comp/.spv/')
.PHONY: all clean all modules $(MODULES)

all: vkdt-cli vkdt modules Makefile

include qvk/flat.mk
include core/flat.mk
include pipe/flat.mk
include gui/flat.mk
include cli/flat.mk

clean: Makefile
	rm -f vkdt vkdt-cli 
	rm -f $(GUI_O) $(CORE_O) $(PIPE_O) $(CLI_O) $(QVK_O)
	rm -f pipe/modules/*/lib*.so

# for sched_affinity
CFLAGS=-Wall -pipe -I. -D_GNU_SOURCE
LDFLAGS=

# TODO: find a generic way to temporarily overwrite CFLAGS and
# TODO: use the default rules instead:

qvk/%.o: qvk/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(QVK_CFLAGS) -c $< -o $@

core/%.o: core/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(CORE_CFLAGS) -c $< -o $@

pipe/%.o: pipe/%.c Makefile $(PIPE_H)
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(PIPE_CFLAGS) -c $< -o $@

cli/%.o: cli/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(CLI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

../ext/imgui/%.o: ../ext/imgui/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

ext/imgui/examples/%.o: ext/imgui/examples/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

# main application
vkdt: $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) Makefile
	$(CC) $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) -o vkdt \
    $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(OPT_LDFLAGS)

# command line interface
vkdt-cli: $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) Makefile
	$(CC) $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) -o vkdt-cli \
    $(LDFLAGS) $(CLI_LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(OPT_LDFLAGS)


modules: $(MODULES) $(SPV) Makefile

pipe/modules/*/Makefile:
	make -C $(dir $@)

%.spv: %.comp
	$(GLSLC) -Ipipe/modules -I$(dir $<) --target-env vulkan1.1 \
    -DQVK_SHADER \
    -V $< -o $@

