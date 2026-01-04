# flat makefile for everything, -j friendly.

# modules which need a dso have flat.mk, collect all these.
# generate list of all modules (the * in pipe/modules/*/flat.mk)
FLATMK:=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/flat.mk))))
MODULES:=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/main.c))))
MODULESXX:=$(notdir $(patsubst %/,%,$(dir $(wildcard pipe/modules/*/main.cc))))
READMES:=$(wildcard pipe/modules/*/readme.md)
# kill some modules based on config flags:
ifeq ($(VKDT_USE_RAWINPUT), 2) # use rawloader
  MODULESXX:=$(filter-out i-raw,$(MODULESXX))
endif
ifeq ($(VKDT_USE_RAWINPUT), 1) # use rawspeed
  MODULES:=$(filter-out i-raw,$(MODULES))
endif
ifeq ($(VKDT_USE_RAWINPUT), 0) # use no raw loading module
  MODULESXX:=$(filter-out i-raw,$(MODULESXX))
  MODULES:=$(filter-out i-raw,$(MODULES))
endif
ifneq ($(VKDT_USE_V4L2), 1)
  MODULES:=$(filter-out i-v4l2,$(MODULES))
endif
ifneq ($(VKDT_USE_MLV), 1)
  MODULES:=$(filter-out i-mlv,$(MODULES))
endif
ifneq ($(VKDT_USE_FFMPEG), 1)
  MODULES:=$(filter-out i-vid o-vid,$(MODULES))
endif
ifneq ($(VKDT_USE_QUAKE), 1)
  MODULES:=$(filter-out quake,$(MODULES))
endif
ifneq ($(VKDT_USE_MCRAW), 1)
  MODULESXX:=$(filter-out i-mcraw,$(MODULESXX))
endif

# generate list of dso filenames, i.e. modules/*/lib*.so according to MODULES
MOD_DSOS:=$(foreach mod,$(MODULES),pipe/modules/$(mod)/lib$(mod).so)
MOD_DSOSXX:=$(foreach mod,$(MODULESXX),pipe/modules/$(mod)/lib$(mod).so)
# also all .comp compute shaders will be compiled to spir-v.
SHD_EXT=comp vert tesc tese geom frag
SPV:=$(foreach shd,$(SHD_EXT),\
$(patsubst %.$(shd),%.$(shd).spv,$(shell ls -1 pipe/modules/*/*.$(shd) 2>/dev/null)))

TOOLTIPS:=$(patsubst %/readme.md,%/ptooltips,$(READMES))

.PHONY: all clean modules

all: ../bin/vkdt-cli ../bin/vkdt-fit ../bin/vkdt modules tools Makefile 

include qvk/flat.mk
include core/flat.mk
include pipe/flat.mk
include db/flat.mk
include snd/flat.mk
include gui/flat.mk
include cli/flat.mk
include fit/flat.mk
include tools/flat.mk

clean: Makefile
	rm -f ../bin/vkdt ../bin/vkdt-cli ../bin/vkdt-fit ../bin/vkdt-read-icc ../bin/vkdt-eval-profile ../bin/vkdt-lutinfo
	rm -f ../bin/vkdt-mkclut ../bin/vkdt-mkssf
	rm -f $(GUI_O) $(CORE_O) $(PIPE_O) $(SND_O) $(CLI_O) $(FIT_O) $(QVK_O) $(DB_O)
	rm -f gui/shd.h
	rm -f gui/shd/*.spv
	# we delete *all* modules, not just the one in MOD_DSOS* because they may be from another branch.
	# such stale libraries can still cause segfaults because they would be loaded.
	# at some point we probably need to harden the api such that this still works. maybe.
	rm -f pipe/modules/*/lib*.so
	rm -f pipe/modules/*/*.spv
	rm -f pipe/modules/*/{ctooltips,ptooltips}

distclean: clean
	rm -f macadam mkabney mkspectra macadam.lut
	rm -rf pipe/modules/i-raw/rawloader-c/target
	rm -rf pipe/modules/i-mcraw/mcraw-*
	rm -rf pipe/modules/i-raw/rawspeed-*
	rm -f core/version.h

# GNU_SOURCE for sched_affinity
CFLAGS?=-Wall -pipe -I. -D_GNU_SOURCE -std=c11
CXXFLAGS?=-Wall -pipe -I. -D_GNU_SOURCE -std=c++17
EXE_CFLAGS?=-fPIC
# LDFLAGS=
VK_CFLAGS:=$(VKDT_VULKAN_CFLAGS)

qvk/%.o: qvk/%.c Makefile $(QVK_H) qvk/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(QVK_CFLAGS) -c $< -o $@

core/%.o: core/%.c Makefile $(CORE_H) core/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(CORE_CFLAGS) -c $< -o $@

snd/%.o: snd/%.c Makefile $(SND_H) snd/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(SND_CFLAGS) -c $< -o $@

pipe/%.o: pipe/%.c Makefile $(PIPE_H) $(CORE_H) pipe/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(PIPE_CFLAGS) -c $< -o $@

db/%.o: db/%.c Makefile $(DB_H) db/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(DB_CFLAGS) -c $< -o $@

cli/%.o: cli/%.c Makefile $(CLI_H) cli/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(CLI_CFLAGS) -c $< -o $@

fit/%.o: fit/%.c Makefile $(FIT_H) fit/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(FIT_CFLAGS) -c $< -o $@

gui/%.o: gui/%.c Makefile $(GUI_H) gui/flat.mk
	$(CC) $(CFLAGS) $(EXE_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

# main application
# ======================
../bin/vkdt: $(GUI_O) $(QVK_O) $(CORE_O) $(SND_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) $(GUI_O) $(QVK_O) $(CORE_O) $(SND_O) $(PIPE_O) $(DB_O) -pie -o $@ \
    $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS) $(SND_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# command line interface
# ======================
../bin/vkdt-cli: $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) -pie -o $@ \
    $(LDFLAGS) $(CLI_LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# parameter optimiser
# ======================
../bin/vkdt-fit: $(FIT_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) $(FIT_O) $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) -pie -o $@ \
    $(LDFLAGS) $(FIT_LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# library
# ======================
../bin/libvkdt.so: $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) Makefile
	$(CC) -shared -nostartfiles -Wl,-soname,libvkdt.so -o $@ $(QVK_O) $(CORE_O) $(PIPE_O) $(DB_O) \
    $(LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS) $(DB_LDFLAGS) $(OPT_LDFLAGS)

# modules
# ======================
reload-shaders: $(SPV) Makefile
modules: $(MOD_DSOS) $(MOD_DSOSXX) $(SPV) $(TOOLTIPS) Makefile

MOD_GLOBAL_DEPS=\
  pipe/modules/api.h\
  pipe/graph.h\
  pipe/module.h\
  pipe/node.h\
  pipe/connector.h\
  pipe/connector.c\
  pipe/connector.inc
MOD_GLOBAL_CFLAGS=-Ipipe -I. -fPIC -DVKDT_DSO_BUILD
MOD_GLOBAL_LDFLAGS=-ldl

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
	$(CC) $(CFLAGS) $(MOD_GLOBAL_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(MOD_$(1)_CFLAGS) -shared $$< $(MOD_$(1)_C) -o $$@ $(LDFLAGS) $(OPT_LDFLAGS) $(MOD_$(1)_LDFLAGS) $(MOD_GLOBAL_LDFLAGS)
endef
$(foreach mod,$(MODULES),$(eval $(call MOD_RULE,$(mod))))

define MOD_RULEXX
pipe/modules/$1/lib$1.so:pipe/modules/$1/main.cc pipe/modules/$1/flat.mk $(MOD_$(1)_DEPS) $(MOD_GLOBAL_DEPS)
	$(CXX) $(CXXFLAGS) $(MOD_GLOBAL_CFLAGS) $(OPT_CFLAGS) $(VK_CFLAGS) $(MOD_$(1)_CFLAGS) -shared $$< $(MOD_$(1)_C) -o $$@ $(LDFLAGS) $(OPT_LDFLAGS) $(MOD_$(1)_LDFLAGS) $(MOD_GLOBAL_LDFLAGS)
endef
$(foreach mod,$(MODULESXX),$(eval $(call MOD_RULEXX,$(mod))))

define SPV_RULE
%.$(1).spv: %.$(1)
	$(GLSLC) -Ipipe/modules -I$(dir $$<) $(GLSLC_FLAGS) $$< -o $$@
endef
$(foreach shd,$(SHD_EXT),$(eval $(call SPV_RULE,$(shd))))

%/ptooltips: %/readme.md Makefile
  # generate tooltips for this module, both for parameters and connectors
	sed -n '/## parameters/,/## / p' $< | sed -n -e '/^* / p' -e '/^  / p' | tr -d '\n' | sed -e 's/\* `/\n/g' -e 's/` */`/g' -e 's/`/:/g' | sed -e '1d' -e '$$a\' > $@
	sed -n '/## connectors/,/## / p' $< | sed -n -e '/^* / p' -e '/^  / p' | tr -d '\n' | sed -e 's/\* `/\n/g' -e 's/` */`/g' -e 's/`/:/g' | sed -e '1d' -e '$$a\' > $(patsubst %ptooltips, %ctooltips, $@)
