# release vkdt 2.0.0

the following is a list of changes over the [previous release 1.0.0](../release-1.0/readme.md).

| <p style="width:25em">&nbsp;</p> | <p style="width:25em">&nbsp;</p> |
|----------------------------|-------------------------|
| ![video](video.png) | **support for vulkan video decode**<br/> speeding up the process substantially. decodes vulkan images straight into the brain of the processing graph. makes working with 6k/hfr footage more fun |
| ![oidn](oidn.jpg) | **open image denoise**<br/> [new module: `oidn`](../../src/pipe/modules/oidn/readme.md), the original intel open image denoise architecture and weights, in vulkan/glsl with fast schedule using [the denox compiler](https://github.com/kistenklaus/denox) |
| ![android](android.jpg) | **runs on android**<br/> through droid deck, use the arm64 appimage. works best with new streamlined gamepad support |
| <video width=100% autoplay muted loop nocontrols><source src="segment.webm"></video> | **automatic segmentation**<br/> new modules: [`segment` for automatic semantic segmentation](../../src/pipe/modules/segment/readme.md) and [`select` to derive masks from user scribbles](../../src/pipe/modules/select/readme.md) (by lennardwalter), based on cnn, efficient execution through denox compiled schedule |
| ![hdr](hdr.jpg) | **better hdr support**<br/> [new module `hdrmerge`](../../src/pipe/modules/hdrmerge/readme.md) to create high dynamic range images from an exposure series (by phcreery), [`o-jxl` to write hdr images](../../src/pipe/modules/o-jxl/readme.md) into jpeg-xl format (by wfinster), and new tonemappers [`reinhard` (by phcreery)](../../src/pipe/modules/reinhard/readme.md) and [the full featured `OpenDRT` (by jedyp)](../../src/pipe/modules/OpenDRT/readme.md), [`filmcurv` module](../../src/pipe/modules/filmcurv/readme.md) enhanced by agx (phcreery) and luminance ratios (randalix) |
| ![menu](menu.png) | **chord menu and drag keys**<br/> for keyboard and gamepad navigation of most ui interactions (by randalix), fully configurable and suitable for full screen operation (panels hidden) |
| ![grade](grade.png) | **new colour wheel widget**<br/> especially for the [revamped `grade` module](../../src/pipe/modules/grade/readme.md) (by randalix) |
| ![gamepad](gamepad.png) | **streamlined gamepad support**<br/> especially fun to use on android. support for chord menu and drag keys, as well as radial menu to pick favourites |
| ![gallery](gallery.png) | **new gallery export css**<br/> minimal noscript/pure-css exporter with new style and better full image view, particularly on mobile screens |
| ![filmsim](filmsim.jpg) | **spektrafilm improvements**<br/>including spectral white balancing and spectral upsampling directly from camera rgb, enabling all in-gamut and matrix-free workflows for difficult illumination. speed improvements and additional features such as preflash (by niklasiivari) |
| ![overlay](overlay.png) | **overlay text**<br/>[new module `overlay`](../../src/pipe/modules/overlay/readme.md) to render text + optional latency measurements on top of the image. useful to annotate [a/b comparisons](../../src/pipe/modules/ab/readme.md) |
| ![graph](graph.jpg) | **graph internals overhaul**<br/>support large memory (buffer address references), `modify` type connectors, new extent negotiation, no more geometry shaders (metal compat), sharper/less noise resampling, pipewire replaced alsa for audio |
| <video width=100% autoplay muted loop nocontrols><source src="screencast.webm"></video> | **screencast mode**<br/> showing all key shortcuts typed echoed on screen, enable in your `config.rc` |
| ![colour](colour.png) | **input colour space support**<br/>for many common cinematic colour spaces (such as aces or pana-v), explicit choice in the `i-vid` module |
| ![clut](clut.jpg) | **better spectral input device transform**<br/>now with multi-step `mkclut` for more accurate white balance interpolation across wider range of correlated colour temperatures (niklasiivari) |
| ![res](resource.png) | **resource tracking for projects**<br/>automatic input file tracking for projects with multiple dependencies, such as multi-image, colour luts, cnn weights, spektrafilm tables, etc. `o-copy` allows you to export them all together for archival purposes |
| ![okgmt](okgmt.png) | **experimental gamut mapping in oklab**<br/>bring back out-of-locus colours with negative energy, in cases where you can't avoid it during input device transform |
| <video width=100% autoplay muted loop nocontrols><source src="inpaint.webm"></video> | **anisotropic inpainting**<br/>horizontal and vertical inpaint modes, great to make things straddling the horizon disappear|
| <video width=100% autoplay muted loop nocontrols><source src="lowlag.webm"></video> | **smoother ui interaction**<br/>via low-lag presentation mode (phcreery) and asynchronous compute for lower-end devices (niklasiivari)|
| ![colour assessment](colass.jpg) | **colour assessment view**<br/>also press `tab` to hide side panels for full screen photo inspection. mapped to l3 on the gamepad (niklasiivari) |


## TODO


jddcnn bayer xtrans intel amd?

hotpx revamp based on noise stats (niklasiivari)

