# rt: simple real-time ray tracing

this uses the vulkan ray tracing extension to 

## connectors

* `geo`    geometry input, binary representation, same as [corona-13](https://github.com/hanatos/corona-13)
* `output` the rendered beauty image
* `blue`   blue noise texture as input lut, as those provided by [christoph](http://momentsingraphics.de/BlueNoise.html)
* `tex`    an array connector for input textures, connect to a [i-jpglst](../i-jpglst/readme.md) module
* `aov`    an output buffer containing some *arbitary output variables* such as fake diffuse albedo or normals

## parameters
this still changes so often.. be careful to trust this text.

* `cam_x` position of the camera
* `cam_w` look-at direction of the camera
* `spp`   samples per pixel
