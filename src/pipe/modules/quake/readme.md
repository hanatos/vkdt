# quake

play the 1996 id software game *quake*.
this input node cannibalises quakespasm to read quake input geometry and textures.
the data is sent to vulkan ray tracing kernels.
you can try `vkdt examples/quake.cfg` to test it. requires a bluenoise input texture, see below.
also it assumes that you have the original game data installed in `/usr/share/games/quake/`.
this module is optional, you need to have compiled with `VKDT_USE_QUAKE=1`, see `config.mk.defaults`.

if you run this through the cli, be sure to give the `--audio` switch, since the audio
callback does gamelogic stuff that can only be done outside the video graph processing.

for instance, render to jpg image and only output the last frame:
```
vkdt-cli --last-frame-only -g examples/quake.cfg --audio quake.aud --format o-jpg --output main --config frames:40 fps:24
```

or, to render out a video directly:
```
vkdt-cli -g examples/quake.cfg --format o-ffmpeg --filename qu.vid --audio qu.aud --output main --config frames:3000 fps:24
ffmpeg -i qu.vid_0002.h264 -f s16le -sample_rate 44100 -channels 2  -i qu.aud -c:v copy quake.mp4
```
(add `-r 60` to resample for different frame rate)
(replace `-c:v copy` by `-vcodec libx264 -crf 27 -preset veryfast` for compression)

## connectors

* `output` the rendered frame, irradiance part. this is not a source because it doesn't require cpu copy (rendered on gpu directly).
* `blue` blue noise input textures. see [this github issue](https://github.com/hanatos/vkdt/issues/32) for an example file.
* `aov` the demodulated albedo, to be multiplied to output after denoising
* `mv` input estimated motion vectors here
* `gbuf` will write gbuffer information here: normal, depth, two moments of irradiance

## parameters

* `cam` custom camera parameters passed as uniforms. these are usually ignored, but you can still grab to play the game.
* `spp` the number of samples per pixel. lower this to gain speed.
* `pause` frame number after which to pause the game. useful for reference renders
* `exec` command string to execute after loading quake

