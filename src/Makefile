.PHONY:all clean
all: vkdt-cli vkdt Makefile

include qvk/flat.mk
include core/flat.mk
include gui/flat.mk
include pipe/flat.mk
include cli/flat.mk

clean: Makefile
	rm -f vkdt vkdt-cli 
	rm -f $(GUI_O) $(CORE_O) $(PIPE_O) $(QVK_O)

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

vkdt: $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) Makefile
	$(CC) $(GUI_O) $(QVK_O) $(CORE_O) $(PIPE_O) -o vkdt $(LDFLAGS) $(QVK_LDFLAGS) $(GUI_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS)

vkdt-cli: $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) Makefile
	$(CC) $(CLI_O) $(QVK_O) $(CORE_O) $(PIPE_O) -o vkdt-cli $(LDFLAGS) $(CLI_LDFLAGS) $(QVK_LDFLAGS) $(PIPE_LDFLAGS) $(CORE_LDFLAGS)
