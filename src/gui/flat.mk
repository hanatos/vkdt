GUI_O=gui/gui.o\
      gui/render.o\
      gui/render_lighttable.o\
      gui/render_darkroom.o\
      gui/render_files.o\
      gui/render_nodes.o\
      gui/main.o\
      gui/view.o
GUI_H=gui/gui.h\
      gui/render.h\
      gui/render_view.h\
      gui/render_darkroom.h\
      gui/api.h\
      gui/api_gui.h\
      gui/font_metrics.h\
      gui/hotkey.h\
      gui/job_copy.h\
      gui/keyaccel.h\
      gui/widget_dopesheet.h\
      gui/widget_draw.h\
      gui/widget_export.h\
      gui/widget_filebrowser.h\
      gui/widget_filteredlist.h\
      gui/widget_tooltip.h\
      gui/widget_image.h\
      gui/widget_nodes.h\
      gui/widget_radial_menu.h\
      gui/widget_radial_menu_dr.h\
      gui/widget_radial_menu_dr-fwd.h\
      gui/widget_recentcollect.h\
      gui/widget_resize_panel.h\
      gui/widget_thumbnail.h\
      gui/view.h\
      gui/darkroom.h\
      gui/lighttable.h\
      gui/files.h\
      gui/nodes.h\
      gui/nuklear.h\
      gui/nuklear_glfw_vulkan.h\
      gui/shd.h\
      core/core.h\
      core/fs.h\
      core/tools.h\
      pipe/graph-traverse.inc\
      pipe/graph-history.h\
      pipe/graph-defaults.h
GUI_CFLAGS=$(VKDT_GLFW_CFLAGS)
GUI_LDFLAGS=-ldl $(VKDT_GLFW_LDFLAGS) -lm $(DYNAMIC)

ifeq ($(VKDT_USE_PENTABLET),1)
GUI_CFLAGS+=-DVKDT_USE_PENTABLET=1
endif

gui/main.o: core/sig.h
gui/shd.h: gui/shd/gui.vert.spv gui/shd/gui.frag.spv
	xxd -i gui/shd/gui.vert.spv > gui/shd.h
	xxd -i gui/shd/gui.frag.spv >> gui/shd.h

%.spv: %
	$(GLSLC) -I$(dir $<).. $(GLSLC_FLAGS) $< -o $@
