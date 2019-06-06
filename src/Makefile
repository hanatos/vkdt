.PHONY:all clean pipe qvk cli pipe-clean qvk-clean cli-clean
all: cli vkdt Makefile

pipe:
	make -C pipe
qvk:
	make -C qvk
cli: qvk pipe
	make -C cli

clean: pipe-clean qvk-clean cli-clean gui-clean Makefile

pipe-clean:
	make -C pipe clean
qvk-clean:
	make -C qvk clean
cli-clean:
	make -C cli clean

include qvk/flat.mk
include core/flat.mk
include gui/flat.mk
include pipe/flat.mk

CFLAGS=-Wall -pipe -I.
LDFLAGS=

# TODO: find a generic way to temporarily overwrite CFLAGS and
# TODO: use the default rules instead:

qvk/%.o: qvk/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(QVK_CFLAGS) -c $< -o $@

core/%.o: core/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(CORE_CFLAGS) -c $< -o $@

pipe/%.o: pipe/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(PIPE_CFLAGS) -c $< -o $@

gui/%.o: gui/%.c Makefile
	$(CC) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

gui/%.o: gui/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

../ext/imgui/%.o: ../ext/imgui/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

ext/imgui/examples/%.o: ext/imgui/examples/%.cpp Makefile
	$(CXX) $(CFLAGS) $(OPT_CFLAGS) $(GUI_CFLAGS) -c $< -o $@

vkdt: $(QVK_O) $(GUI_O) $(CORE_O) Makefile
	$(CC) $(QVK_O) $(GUI_O) $(CORE_O) $(PIPE_O) -o vkdt $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS)
