# how to do basic things in the gui

## views

the basic cascade of ui views is:

files ⟷ lighttable ⟷ darkroom ⟷ node editor

you can usually walk the arrows by key accel (enter/escape) or gamepad
(`x`/`o`). the way from darkroom to node editor is special, because you will
not technically leave darkroom mode. the default key accel from darkroom to
node editor is `ctrl-n`.

in general, you can export your work but don't have to save it.

the processing history `.cfg` is saved when you exit darkroom mode or switch to
the next image (backspace/space or L1/R1).

labels and ratings `vkdt.db` are saved when you switch folder or exit vkdt.

the positions of the 2D node graph layout are saved to disk when you exit
the node editor view.

### files

[![file manager](files.jpg)](files.jpg)

this is a basic file manager which allows you to navigate to folders and
to mount external drives and import images from them.

### lighttable

[![lighttable](lighttable.jpg)](lighttable.jpg)

in this view you can look at a folder of images. you can do basic sorting and
filtering (see widgets in the right panel). the images listed in one lighttable
session always correspond directly to a folder on disk. collections formed by
tags are symlinks in a folder in `~/.config/vkdt/tags/`.
in particular this means there is no obscure database that holds references
to your images and needs to be synchronised.
you can copy/paste edit history between images and export a selection here.

### darkroom

[![darkroom](darkroom.jpg)](darkroom.jpg)
[![darkroom fullscreen](darkroom-fs.jpg)](darkroom-fs.jpg)

this allows you to edit the image. press `tab` for a fullscreen mode that only
shows the image (bottom), not the controls in the right panel. press `ctrl-h`
(default hotkey) to show the edit history panel on the left which also allows
you to roll back changes.

### node editor

[![node editor](node-editor.jpg)](node-editor.jpg)

for complex editing graphs it is useful to view them in a more generous 2D
layout. the node editor offers most screen real estate to the node graph and,
as a trade off, only shows small images in the right panel.


## rate, label, tag

colour labels `f1`-`f5`

star ratings `1`-`5`

tag `ctrl-t`

these key accels work in both lighttable mode (on the whole selection) and in
darkroom mode (only on the single edited image).

## hotkeys

can be customised for each view separately in the gui via `settings`→
`hotkeys`. some keys are special and not in the hotkey system.

`f11` goes full screen

`ctrl+q` quits the application

## imgui widgets

most widgets are stock imgui widgets. in particular, if you want to
type a value instead of clicking, the way to do it is to `ctrl+click`.
most widgets support a double-click-to-reset-to default action.
