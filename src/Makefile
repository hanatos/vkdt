.PHONY:all clean pipe qvk cli gui pipe-clean qvk-clean cli-clean gui-clean
all: cli vkdt Makefile

pipe:
	make -C pipe
qvk:
	make -C qvk
cli: qvk pipe
	make -C cli
gui: qvk pipe
	make -C gui

clean: pipe-clean qvk-clean cli-clean gui-clean Makefile

pipe-clean:
	make -C pipe clean
qvk-clean:
	make -C qvk clean
cli-clean:
	make -C cli clean
gui-clean:
	make -C gui clean

include qvk/flat.mk
include core/flat.mk
include gui/flat.mk
include pipe/flat.mk

CFLAGS=-Wall -pipe -I.
LDFLAGS=

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

vkdt: $(QVK_O) $(GUI_O) $(CORE_O) Makefile
	$(CC) $(QVK_O) $(GUI_O) $(CORE_O) $(PIPE_O) -o vkdt $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS)
