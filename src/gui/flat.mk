GUI_O=gui/gui.o\
      gui/render.o\
      gui/render_files.o\
      gui/render_lighttable.o\
      gui/render_darkroom.o\
      gui/darkroom.o\
      gui/main.o\
      gui/view.o\
      ../ext/imgui/imgui.o\
      ../ext/imgui/imgui_draw.o\
      ../ext/imgui/imgui_widgets.o\
      ../ext/imgui/imgui_tables.o\
      ../ext/imgui/backends/imgui_impl_vulkan.o\
      ../ext/imgui/backends/imgui_impl_glfw.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/render_view.hh\
      gui/api.h\
      gui/api.hh\
      gui/hotkey.hh\
      gui/widget_filebrowser.hh\
      gui/widget_thumbnail.hh\
      gui/widget_recentcollect.hh\
      gui/widget_filteredlist.hh\
      gui/view.h\
      gui/darkroom.h\
      gui/darkroom-util.h\
      gui/lighttable.h\
      core/fs.h\
      pipe/graph-traverse.inc\
      pipe/graph-history.h
GUI_CFLAGS=$(VKDT_GLFW_CFLAGS) -I../ext/imgui -I../ext/imgui/backends/
GUI_LDFLAGS=-ldl $(VKDT_GLFW_LDFLAGS) -lm -lstdc++

ifeq ($(VKDT_USE_FREETYPE),1)
GUI_CFLAGS+=$(shell pkg-config --cflags freetype2) -DVKDT_USE_FREETYPE=1
GUI_LDFLAGS+=$(shell pkg-config --libs freetype2)
endif
ifeq ($(VKDT_USE_PENTABLET),1)
GUI_CFLAGS+=-DVKDT_USE_PENTABLET=1
endif

gui/main.o:core/version.h core/signal.h
