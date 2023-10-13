# how to change the default graph

vkdt ships with a couple of default processing graphs. these are defined in the
installation directory as `default.i-*` and `default-darkroom.i-*`. you can
override these by placing a (modified) copy in your `~/.config/vkdt` directory.
the suffix is the name of the input module used for certain file types, for
instance `i-jpg` for jpeg image files. the file itself contains a human
readable `.cfg` file, so you can edit an image to taste and just place the
resulting `.cfg` file there as default.
the filename of the individual input files will be appended to the config.

the `-darkroom` infix means that it is used when entering darkroom mode. this enables
you to use faster processing/take shortcuts when rendering thumbnails.
