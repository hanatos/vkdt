# adding support for new file formats

the modular architecture makes it easy to add and remove
support for additional file formats (such as jpeg, jxl, ..).
there are a few additional steps required to make integration smooth.

## output / writing

this is as simple as creating an `o-new` directory and module. the export
widget will pick it up transparently (because of the `o-*` name). follow the
examples in the `pipe/modules/` subdirectory.

## input / reading

of course this requires an `i-new` module. this should be
straight forward to create following one of the examples.

for smooth integration, vkdt should also pick up these
images and associate default graphs with the new input module
to the correct files.

* `src/pipe/graph-defaults.h`: registry to map file extension to input module
* `bin/default.i-*`: default processing graph for thumbnails with given input module
* `bin/default-darkroom.i-*`: default processing graph when entering darkroom mode
* `src/gui/api-gui.h` `dt_gui_paste_history()`: take care of correct input module when copy/pasting history between images
