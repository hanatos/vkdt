# o-web: pseudo module to export videos or jpg

this module does nothing but will be replaced during export. if the graph has
more frames than a still image, the `o-vid` module will be used. otherwise this
will be `o-jpg`. use in conjunction with `vkdt gallery` to create a website.

note that the gui will also setup dual output to write the jpg/mp4 as well as
a small thumbnail image (forked from the same processing pipeline). these are
used by `vkdt gallery` to show the overview grid of images.
