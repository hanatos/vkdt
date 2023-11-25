GUI_O=gui/gui.o\
      gui/render.o\
      gui/render_files.o\
      gui/render_lighttable.o\
      gui/render_darkroom.o\
      gui/render_nodes.o\
      gui/darkroom.o\
      gui/main.o\
      gui/view.o\
      gui/imnodes.o\
      ../ext/imgui/imgui.o\
      ../ext/imgui/imgui_draw.o\
      ../ext/imgui/imgui_widgets.o\
      ../ext/imgui/imgui_tables.o\
      ../ext/imgui/backends/imgui_impl_vulkan.o\
      ../ext/imgui/backends/imgui_impl_glfw.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/render_view.hh\
      gui/render_darkroom.hh\
      gui/api.h\
      gui/api.hh\
      gui/hotkey.hh\
      gui/keyaccel.hh\
      gui/widget_dopesheet.hh\
      gui/widget_draw.hh\
      gui/widget_export.hh\
      gui/widget_filebrowser.hh\
      gui/widget_filteredlist.hh\
      gui/widget_image.hh\
      gui/widget_recentcollect.hh\
      gui/widget_thumbnail.hh\
      gui/view.h\
      gui/darkroom.h\
      gui/lighttable.h\
      gui/files.h\
      gui/nodes.h\
      core/core.h\
      core/fs.h\
      core/tools.h\
      pipe/graph-traverse.inc\
      pipe/graph-history.h\
      pipe/graph-defaults.h
GUI_CFLAGS=$(VKDT_GLFW_CFLAGS) -I../ext/imgui -I../ext/imgui/backends/
GUI_LDFLAGS=-ldl $(VKDT_GLFW_LDFLAGS) -lm -lstdc++ -rdynamic

ifeq ($(VKDT_USE_FREETYPE),1)
GUI_CFLAGS+=$(VKDT_FREETYPE2_CFLAGS) -DVKDT_USE_FREETYPE=1
GUI_LDFLAGS+=$(VKDT_FREETYPE2_LDFLAGS)
endif
ifeq ($(VKDT_USE_PENTABLET),1)
GUI_CFLAGS+=-DVKDT_USE_PENTABLET=1
endif

gui/main.o: core/signal.h
