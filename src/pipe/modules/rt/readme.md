# rt: simple real-time ray tracing

this uses the vulkan ray tracing extension to path trace a scene. the geometry comes from
one or more [`bvh` modules](../bvh/readme.md) which collect input for bottom level acceleration structures.

the `rt` module uses hero wavelength spectral sampling for accurate colour reproduction.

[![ray traced scene](rt.jpg)](rt.jpg)

(scene: [nikita sky demo](https://cloud.blender.org/p/gallery/5f4d1791cc1d7c5e0e8832d4))

## connectors

* `output` the rendered beauty image
* `blue`   blue noise texture as input lut, as those provided by [christoph](http://momentsingraphics.de/BlueNoise.html)
* `tex`    an array connector for input textures, connect to a [i-jpglst](../i-jpglst/readme.md) module
* `aov`    an output buffer containing some *arbitary output variables* such as fake diffuse albedo or normals

## parameters
this still changes so often.. be careful to trust this text.

* `cam` position, direction, and up vector of the camera
* `fog` homogeneous global background fog
* `spp` samples per pixel
* `envmap` filename of the environment map, 2:1 aspect latitude/longitude, .hdr preferably 8k
* `cam mode` use custom camera in this module, or grab camera, simulation time, and fog from another module
* `cam_mod` if cam mode is set to another module, this is the other module's name
* `cam_inst` if cam mode is set to another module, this is the other module's instance
* `sampler` choose which path space sampler to use for rendering
