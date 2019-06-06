GUI_O=gui/gui.o\
      gui/render.o\
      gui/main.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/imgui_impl_sdl.h\
      gui/imgui_impl_vulkan.h
GUI_CFLAGS=$(shell pkg-config --cflags sdl2) -I../ext/imgui
GUI_LDFLAGS=-ldl $(shell pkg-config --libs sdl2)
