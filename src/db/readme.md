# image database

vkdt's database backend is fairly minimal. in particular:

* it does not copy around your original data.
* it does not write to your original data.
* it merely writes `*.cfg` sidecar files with processing info.  
  in fact these are the actual input files to a loaded directory.
  there is, however, a fallback to known raw files which will spawn
  default `.cfg` files by simply appending the suffix to the full name.
* it writes a minimal `vkdt.db` file to each directory, containing  
  information about rating and labels.


## thumbnails

vkdt stores thumbnails for lighttable view in `.cache/vkdt/<murmur3-hash>.bc1`.
that is, they are compressed in bc1 format on the fly and also stored as such
on disk. this is good for fast and compact display on gpu.

## tags/collections

you can assign *tags* or images to *named collections* in lighttable mode. this
will create (if it doesn't already exist) a directory in
`.config/vkdt/tags/<tagname>/` and create a symlink `.cfg` file in this
directory, pointing to the images you assigned the tag to. you can then open
all images with the given tag by pointing `vkdt` to this directory.

a collection created this way has its own `vkdt.db` file, so you can assign a
different rating or labels when working on this collection. this means there is
no concept of global rating/labels, but these are relative to the directory you
are working on. this facilitates relative ratings (best out of a collection of
all 5-star images for instance).

currently there is no (fast) way to see all labels assigned to a particular
image.
