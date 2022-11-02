# quake

play the 1996 id software game *quake*.
this input node cannibalises quakespasm to read quake input geometry and textures.
the data is sent to vulkan ray tracing kernels.
you can try `vkdt examples/quake.cfg` to test it. requires a bluenoise input texture, see below.
also it assumes that you have the original game data installed in `/usr/share/games/quake/`.
this module is optional, you need to have compiled with `VKDT_USE_QUAKE=1`, see `config.mk.defaults`.

## connectors

* `output` the rendered frame. this is not a source because it doesn't require cpu copy (rendered on gpu directly).
* `blue` blue noise input textures. see [this github issue](https://github.com/hanatos/vkdt/issues/32) for an example file.
* `aov` the arbitrary output variables channel. currently writes diffuse albedo.

## parameters

* `cam` custom camera parameters passed as uniforms. these are usually ignored, but you can still grab to play the game.
* `spp` the number of samples per pixel. lower this to gain speed.

