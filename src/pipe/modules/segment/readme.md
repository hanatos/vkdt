# segment: automatic masks

computes a soft mask of a class of image content, using a neural network, no
clicks needed. `channel` chooses what to select: people, sky, vegetation,
water, mountain, ground (natural: soil, sand, snow, rock), building, the main
subject of the photo, artificial ground (roads, pavement, floors, railways,
sports fields) or clothing. `invert` selects everything else, e.g. the
background when used with `subject`. use the mask as the `mask` input of a
[blend module](../blend/readme.md), refined by a [guided filter](../guided/readme.md).
the `segment-sky` preset shows how.

the weights are read from `~/.config/vkdt/data/segment-nseg.dat`. only nvidia
gpus are supported so far, without weights the mask is empty.
you can [download the weights package here](https://codeberg.org/hanatos/vkdt-nn/releases).

## parameters

* `channel` what the mask selects
* `invert` select everything else

## connectors

* `input` scene-referred linear rec2020
* `output` the mask

## ethics

the network was trained from scratch on openly licensed images only: the
subset of [COCO-Stuff](https://github.com/nightrome/cocostuff) whose images are
CC BY, CC BY-SA or have no known copyright restrictions, and the segmentation
masks of [Open Images V7](https://storage.googleapis.com/openimages/web/index.html)
(annotations CC BY 4.0, images CC BY 2.0). no pretrained weights were used and
no images are distributed. the subject channel is learned from the largest,
most central object of the COCO images. Open Images point labels (e.g. sky,
tree, road) add sparse supervision.

## technical

the spir-v is generated using [denox](https://github.com/kistenklaus/denox) and
[vkdt-denox](https://github.com/kistenklaus/vkdt-denox).
