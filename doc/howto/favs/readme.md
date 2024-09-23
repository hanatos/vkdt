# how to customise the favourites tab in darkroom mode

find `darkroom.ui` in the vkdt `data/` folder, copy it to
`~/.config/vkdt/darkroom.ui` and edit to taste. `darkroom.ui` is read from
vkdt's `data/` directory, and the version in your home will take precedence if
it's found. this holds true for most configuration files such as presets,
display profiles, etc.

every line represents one ui element, and the file determines the order in
which these will appear in the favourites ui tab. the first two strings
identify the module and instance, if these aren't on your current graph, the
element will not be shown.

the third is the name of the parameter, the fourth identifies the ui element
used to change the parameter, followed by optional arguments for the ui element
(slider ranges, which you can btw always override by mouse clicking the
number+typing).

these ui widget descriptions are mostly a straight copy from the `params.ui` file
in the respective module subdirectories.
