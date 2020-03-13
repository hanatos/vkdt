# flat makefile for everything, -j friendly.

# modules which need a dso have flat.mk, collect all these.
# generate list of all modules (the * in pipe/modules/*/flat.mk)
FLATMK=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/flat.mk))))
MODULES=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/main.c))))
MODULESXX=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/main.cc))))
# generate list of dso filenames, i.e. modules/*/lib*.so according to MODULES
MOD_DSOS=$(foreach mod,$(MODULES),pipe/modules/$(mod)/lib$(mod).so)
MOD_DSOSXX=$(foreach mod,$(MODULESXX),pipe/modules/$(mod)/lib$(mod).so)
# also all .comp compute shaders will be compiled to spir-v.
SHD_EXT=comp vert tesc tese geom frag
SPV=$(foreach shd,$(SHD_EXT),\
$(patsubst %.$(shd),%.$(shd).spv,$(shell ls -1 pipe/modules/*/*.$(shd))))


.PHONY: all clean modules

all: vkdt-cli vkdt modules Makefile

include qvk/flat.mk
include core/flat.mk
include pipe/flat.mk
include db/flat.mk
include gui/flat.mk
include cli/flat.mk

clean: Makefile
	rm -f vkdt vkdt-cli 
	rm -f $(GUI_O) $(CORE_O) $(PIPE_O) $(CLI_O) $(QVK_O) $(DB_O)
	rm -f $(MOD_DSOS) $(MOD_DSOSXX)
	rm -f pipe/modules/*/*.spv

# for sched_affinity
CFLAGS=-Wall -pipe -I. -D_GNU_SOURCE
LDFLAGS=

qvk/%.o: qvk/%.c Makefile $(QVK_H) qvk/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(QVK_CFLAGS) -c $< -o $@

core/%.o: core/%.c Makefile $(CORE_H) core/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(CORE_CFLAGS) -c $< -o $@

pipe/%.o: pipe/%.c Makefile $(PIPE_H) pipe/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(PIPE_CFLAGS) -c $< -o $@

db/%.o: db/%.c Makefile $(DB_H) db/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(DB_CFLAGS) -c $< -o $@

cli/%.o: cli/%.c Makefile $(CLI_H) cli/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(CLI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.c Makefile $(GUI_H) gui/flat.mk
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.cc Makefile $(GUI_H) gui/flat.mk
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

../ext/imgui/%.o: ../ext/imgui/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

ext/imgui/examples/%.o: ext/imgui/examples/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

# main application
# ======================
vkdt: $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) -o vkdt \
    $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# command line interface
# ======================
vkdt-cli: $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) -o vkdt-cli \
    $(LDFLAGS) $(CLI_LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# modules
# ======================
modules: $(MOD_DSOS) $(MOD_DSOSXX) $(SPV) Makefile

MOD_GLOBAL_DEPS=\
  pipe/modules/api.h\
  pipe/graph.h\
  pipe/module.h\
  pipe/node.h\
  pipe/connector.h\
  pipe/connector.c\
  pipe/connector.inc
MOD_GLOBAL_CFLAGS=-Ipipe -I. -fPIC

# expand all cflags/ldflags/deps/extra-c from flat.mk:
$(foreach MOD,$(FLATMK),\
  $(eval MOD_CFLAGS=)\
  $(eval MOD_LDFLAGS=)\
  $(eval MOD_DEPS=)\
  $(eval MOD_C=)\
  $(eval include pipe/modules/$(MOD)/flat.mk)\
  $(eval MOD_$(MOD)_CFLAGS=$(MOD_CFLAGS))\
  $(eval MOD_$(MOD)_LDFLAGS=$(MOD_LDFLAGS))\
  $(eval MOD_$(MOD)_DEPS=$(MOD_DEPS))\
  $(eval MOD_$(MOD)_C=$(MOD_C)))

define MOD_RULE
pipe/modules/$1/lib$1.so: pipe/modules/$1/main.c pipe/modules/$1/flat.mk $(MOD_$(1)_DEPS) $(MOD_GLOBAL_DEPS)
	$(CC) $(CFLAGS) $(MOD_GLOBAL_CFLAGS) $(OPT_CFLAGS) $(MOD_$(1)_CFLAGS) -shared $$< $(MOD_$(1)_C) -o $$@ $(LDFLAGS) $(MOD_$(1)_LDFLAGS)
endef
$(foreach mod,$(MODULES),$(eval $(call MOD_RULE,$(mod))))

define MOD_RULEXX
pipe/modules/$1/lib$1.so:pipe/modules/$1/main.cc pipe/modules/$1/flat.mk $(MOD_$(1)_DEPS) $(MOD_GLOBAL_DEPS)
	$(CXX) $(CXXFLAGS) $(MOD_GLOBAL_CFLAGS) $(OPT_CFLAGS) $(MOD_$(1)_CFLAGS) -shared $$< $(MOD_$(1)_C) -o $$@ $(LDFLAGS) $(MOD_$(1)_LDFLAGS)
endef
$(foreach mod,$(MODULESXX),$(eval $(call MOD_RULEXX,$(mod))))

define SPV_RULE
%.$(1).spv: %.$(1)
	$(GLSLC) -Ipipe/modules -I$(dir $$<) --target-env vulkan1.1 \
    -V $$< -o $$@
endef
$(foreach shd,$(SHD_EXT),$(eval $(call SPV_RULE,$(shd))))

