# release vkdt 2.0.0

the following is a list of changes over the [previous release 1.0.0](../release-1.0/readme.md).

| <p style="width:25em">&nbsp;</p> | <p style="width:25em">&nbsp;</p> |
|----------------------------|-------------------------|
| ![video](video.png) | **support for vulkan video decode**<br/> speeding up the process substantially. decodes vulkan images straight into the brain of the processing graph. makes working with 6k/hfr footage more fun |
| ![oidn](oidn.jpg) | **open image denoise**<br/> [new module: `oidn`](../../src/pipe/modules/oidn/readme.md), the original intel open image denoise architecture and weights, in vulkan/glsl with fast schedule using [the denox compiler](https://github.com/kistenklaus/denox) |
| ![android](android.jpg) | **runs on android**<br/> through droid deck, use the arm64 appimage. works best with new streamlined gamepad support |
| ![segment](segment.webp) | **automatic segmentation**<br/> new modules: [`segment` for automatic semantic segmentation](../../src/pipe/modules/segment/readme.md) and [`select` to derive masks from user scribbles](../../src/pipe/modules/select/readme.md) (by lennardwalter), based on cnn, efficient execution through denox compiled schedule |
| ![hdr](hdr.jpg) | **better hdr support**<br/> [new module `hdrmerge`](../../src/pipe/modules/hdrmerge/readme.md) to create high dynamic range images from an exposure series (by phcreery), [`o-jxl` to write hdr images](../../src/pipe/modules/o-jxl/readme.md) into jpeg-xl format (by wfinster), and new tonemappers [`reinhard` (by phcreery)](../../src/pipe/modules/reinhard/readme.md) and [the full featured `OpenDRT` (by jedyp)](../../src/pipe/modules/OpenDRT/readme.md), [`filmcurv` module](../../src/pipe/modules/filmcurv/readme.md) enhanced by agx (phcreery) and luminance ratios (randalix) |
| ![menu](menu.png) | **chord menu and drag keys**<br/> for keyboard and gamepad navigation of most ui interactions (by randalix), fully configurable and suitable for full screen operation (panels hidden) |
| ![grade](grade.png) | **new colour wheel widget**<br/> especially for the [revamped `grade` module](../../src/pipe/modules/grade/readme.md) (by randalix) |
| ![gamepad](gamepad.png) | **streamlined gamepad support**<br/> especially fun to use on android. support for chord menu and drag keys, as well as radial menu to pick favourites |
| ![gallery](gallery.png) | **new gallery export css**<br/> minimal noscript/pure-css exporter with new style and better full image view, particularly on mobile screens |
| ![filmsim](filmsim.jpg) | **spektrafilm improvements**<br/>including spectral white balancing and spectral upsampling directly from camera rgb, enabling all in-gamut and matrix-free workflows for difficult illumination. speed improvements and additional features such as preflash (by niklasiivari) |
| ![overlay](overlay.png) | **overlay text**<br/>[new module `overlay`](../../src/pipe/modules/overlay/readme.md) to render text + optional latency measurements on top of the image. useful to annotate [a/b comparisons](../../src/pipe/modules/ab/readme.md) |
| ![graph](graph.jpg) | **graph internals overhaul**<br/>support large memory (buffer address references), `modify` type connectors, new extent negotiation, no more geometry shaders (metal compat), sharper/less noise resampling |

## TODO

okmgtclp


low-lag (phcreery) async compute (niklasiivari)

external resource tracking

mkclut and white balance by temperature

inpaint and straight horizon

jddcnn bayer xtrans intel amd?

input colour spaces (pana v, ..)

removed resnet

pipewire replaced alsa

screencast mode
