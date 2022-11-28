# display: an output for gui view or export

this module implements means to display output pixels on screen.
in expects rec2020 linear data and will colour manage it for
your screen in the gui.

a special `display:main` module will fill the center space in darkroom mode.
the display named `hist` will show in the top right corner, and `view0` just
below it, if it is found.

these displays can be selected in [`vkdt-cli` on the command
line](../../../cli/readme.md), and be replaced by file output nodes
individually.

## connectors

* `input` the sink connector receiving the data
