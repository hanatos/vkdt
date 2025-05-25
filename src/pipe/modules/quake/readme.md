# quake: play the 1996 id software game

this input node cannibalises quakespasm to read quake input geometry and
textures. the data is provided as output connectors, to be used with the [`rt` module](../rt/readme.md).
please see the example in `examples/ad.cfg` for a test setup.
the `quake` module assumes that you have the original game data installed in
`/usr/share/games/quake/` or `.quakespasm/`. this module is optional, you need to have compiled
with `VKDT_USE_QUAKE=1`, see `config.mk.defaults`.

for added fun, you may want to install the excellent [*arcane dimensions*
mod](https://www.moddb.com/mods/arcane-dimensions). it installs additional
`pak` files to `/usr/share/games/ad/` or `.quakespasm/ad/`. for more detailed textures, get the hd
textures from the [*quake revitalisation
project*](https://www.moddb.com/mods/quake-hd-pack-guide/addons/quake-revitalization-project-texture-pack).
place these in the `/usr/share/games/ad` subdirectory too, use consecutive
numbers in the pak filenames. i don't know how these interact with the
newfangled re-release of quake, so be sure to use the original game paks from
your 1996 cdrom in your drawer.
the `examples/ad.cfg` example assumes arcane dimensions is installed (much nicer more complex and modern maps).


## cli operation

render to jpg image and only output the last frame:
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

* `ui` renders some ui (health bar) to be re-composited on the final render
* `dyngeo` dynamic geometry, to be wired to a bvh module
* `stcgeo` static geometry, to be wired to a bvh module
* `tex` the texture array, to be wired to the rt module

## parameters

* `cam` grab the mouse/keyboard input to play the game
* `pause` frame number after which to pause the game. useful for reference renders. will also set a fixed quake time per frame. zero means no pause
* `exec` command string to execute after loading quake. use with cli/config files. accepts ';' separated lists of commands

