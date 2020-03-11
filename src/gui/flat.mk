GUI_O=gui/gui.o\
      gui/render.o\
      gui/main.o\
      gui/view.o\
      ../ext/imgui/imgui.o\
      ../ext/imgui/imgui_draw.o\
      ../ext/imgui/imgui_widgets.o\
      ../ext/imgui/examples/imgui_impl_vulkan.o\
      ../ext/imgui/examples/imgui_impl_sdl.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/view.h\
      gui/darkroom.h\
      pipe/graph-traverse.inc
GUI_CFLAGS=$(shell pkg-config --cflags sdl2) -I../ext/imgui -I../ext/imgui/examples/
GUI_LDFLAGS=-ldl $(shell pkg-config --libs sdl2) -lm -lstdc++

ifeq ($(VKDT_USE_FREETYPE),1)
GUI_CFLAGS+=$(shell pkg-config --cflags freetype2) -DVKDT_USE_FREETYPE=1
GUI_LDFLAGS+=$(shell pkg-config --libs freetype2)
endif
