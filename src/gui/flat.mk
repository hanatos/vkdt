# TODO: remove demo if not needed:
GUI_O=gui/gui.o\
      gui/render.o\
      gui/main.o\
      ../ext/imgui/imgui.o\
      ../ext/imgui/imgui_draw.o\
      ../ext/imgui/imgui_widgets.o\
      ../ext/imgui/imgui_demo.o\
      ../ext/imgui/examples/imgui_impl_vulkan.o\
      ../ext/imgui/examples/imgui_impl_sdl.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/imgui_impl_sdl.h\
      gui/imgui_impl_vulkan.h
GUI_CFLAGS=$(shell pkg-config --cflags sdl2) -I../ext/imgui -I../ext/imgui/examples/
GUI_LDFLAGS=-ldl $(shell pkg-config --libs sdl2) -lm -lstdc++
