# quake: play the 1996 id software game

this input node cannibalises quakespasm to read quake input geometry and
textures. the data is sent to vulkan ray tracing kernels. you can try `vkdt
examples/quake.cfg` to test it. requires a bluenoise input texture, see below.
also it assumes that you have the original game data installed in
`/usr/share/games/quake/`. this module is optional, you need to have compiled
with `VKDT_USE_QUAKE=1`, see `config.mk.defaults`.

for added fun, you may want to install the excellent [*arcane dimensions*
mod](https://www.moddb.com/mods/arcane-dimensions). it installs additional
`pak` files to `/usr/share/games/ad`. for more detailed textures, get the hd
textures from the [*quake revitalisation
project*](https://www.moddb.com/mods/quake-hd-pack-guide/addons/quake-revitalization-project-texture-pack).
place these in the `/usr/share/games/ad` subdirectory too, use consecutive
numbers in the pak filenames. i don't know how these interact with the
newfangled re-release of quake, so be sure to use the original game paks from
your 1996 cdrom in your drawer. to run arcane dimensions, use the
`examples/ad.cfg` file.

## algorithm

the rendering core implements guided importance sampling as described in the paper  
[*Markov Chain Mixture Models for Real-Time Direct Illumination*.](https://jo.dreggn.org/home/2023_mcmm.pdf)  
Dittebrandt et al., CGF (Eurographics Symposium on Rendering), 42(4), 2023.

## cli operation

if you run this through the cli, be sure to pass `--audio` (before the
`--output` option), since the audio callback does gamelogic stuff (worldspawn)
that can only be done outside the video graph processing.

for instance, render to jpg image and only output the last frame:
```
vkdt cli --last-frame-only -g examples/quake.cfg --audio quake.aud --format o-jpg \
  --output main --config frames:40 fps:24
```

or, to render out a video directly:
```
vkdt cli -g examples/quake.cfg --format o-ffmpeg --filename qu.vid --audio qu.aud \
  --output main --config frames:3000 fps:24
ffmpeg -i qu.vid_0001.h264 -f s16le -sample_rate 44100 -channels 2  -i qu.aud \
  -c:v copy quake.mp4
```
(add `-r 60` before `-i`to resample for different frame rate)
(replace `-c:v copy` by `-vcodec libx264 -crf 27 -preset veryfast` for compression)

you probably want to use the [`svgf` module](../svgf/readme.md) to remodulate
the albedo and apply temporal anti aliasing after this module.

## screenshots

these are taken with the hd textures and the ad mod, using an rtx 2080ti device.

[![e1m2](q0.png)](q0.png)
[![end](q1.png)](q1.png)
[![ad_tears](q2.png)](q2.png)

## connectors

* `output` the rendered frame, irradiance part. this is not a source because it doesn't require cpu copy (rendered on gpu directly)
* `blue` blue noise input textures. see [this github issue](https://github.com/hanatos/vkdt/issues/32) for an example file
* `aov` the demodulated albedo, to be multiplied to output after denoising
* `mv` input estimated motion vectors here, see the [`align` module](../align/readme.md)
* `gbuf` will write gbuffer information here: normal, depth, two moments of irradiance
* `debug` randomly changing debug pixels, as i change my mind

## parameters

* `cam` grab the mouse/keyboard input to play the game
* `pause` frame number after which to pause the game. useful for reference renders. will also set a fixed quake time per frame. zero means no pause
* `exec` command string to execute after loading quake. use with cli/config files. accepts ';' separated lists of commands
* `ref` reference render mode. either use guided vMF mixture models or reference/plain cosine hemisphere sampling
* `wd` the width of the rendered image in pixels
* `ht` the height of the rendered image in pixels

