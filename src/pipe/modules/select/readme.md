# select: click to select

computes a soft mask of an object you click on, using a neural network. press
`select`, then left click on the image to add to the selection and right click
to remove from it, or drag a box around the object. with a box the network only
sees the box and a margin around it, zoomed in, and the mask is empty outside of
that. with `draw start` you can instead roughly paint over the object, the
network turns the strokes into a clean mask. strokes, clicks and a box can be
combined.

the network runs once per click and gets the mask of the run before as input,
so later clicks refine the selection. with many clicks, the first run gets the
first click and the four runs after it the latest clicks. if the object is
small, the network runs once more zoomed in on a region around the mask, for
finer edges.

use the mask as the `mask` input of a [blend module](../blend/readme.md),
refined by a [guided filter](../guided/readme.md). the `select-exposure` preset
shows how.

the weights are read from `~/.config/vkdt/data/select-nclk.dat`. only nvidia
gpus are supported so far, without weights the mask is empty.
you can [download the weights package here](https://codeberg.org/hanatos/vkdt-nn/releases).

## parameters

* `click` the number of clicks, followed by x, y and 1 (add) or 0 (remove) for
  each click, in coordinates of the input image
* `box` x0 y0 x1 y1 of a box around the object in coordinates of the input image,
  an empty box means none
* `draw` brush strokes, see the [draw module](../draw/readme.md)
* `radius` the brush radius as fraction of the image width
* `zoom` run the network a second time zoomed in on small objects

## connectors

* `input` scene-referred linear rec2020
* `output` the mask

## ethics

the network was trained from scratch on the segmentation masks and annotator
clicks of [Open Images V7](https://storage.googleapis.com/openimages/web/index.html)
(annotations CC BY 4.0, images CC BY 2.0). no pretrained weights were used and
no images are distributed.

## technical

the spir-v is generated using [denox](https://github.com/kistenklaus/denox) and
[vkdt-denox](https://github.com/kistenklaus/vkdt-denox).
