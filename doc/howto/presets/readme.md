# how to manage presets

in a way, presets are a simplified version of [blocks](../blocks/readme.md).
unlike in darktable, in vkdt presets can apply changes to the processing graph
globally, not just to a single module. unlike blocks, presets are not a
template but a simple list of commands to be applied to the graph. this
includes changing parameters, inserting new modules, and performing connections
between modules.

vkdt ships a few carefully orchestrated presets in the `data/presets`
subdirectory of the installation. these have been created with [the default
processing](../defgraph/readme.md) in mind, for instance consider
`inpaint.pst`:
```
module:inpaint:inpaint
module:draw:inpaint
connect:draw:inpaint:output:inpaint:inpaint:mask
connect:demosaic:01:output:inpaint:inpaint:input
connect:inpaint:inpaint:output:crop:01:input
```
which adds two new modules and inserts them between demosaic and crop, assuming
that these two come directly after each other in the existing graph:

![nodegraph](inpaint.jpg)

of course this may fail to apply on heavily customised graphs.

custom user presets can be created and are located in `~/.config/vkdt/presets`.

## create a custom preset

you can create your own preset by using the gui (default hotkey `ctrl-o`).
if you'd rather use a text editor, you can also copy the `.cfg` of an image
to `~/.config/vkdt/presets/` and delete all lines you don't want to be
included in the preset.

## applying a preset

the default hotkey in darkroom mode is `ctrl-p` and will bring up the menu
to select a preset from a list.
