# ffmpeg video output

this is a `cli` output module, don't use it in the gui! the gui may perform
frame drops, especially when the outputs need to be written out and are encoded
as we go.

we use plain `popen()` style communication with the ffmpeg binary (you'll need
to have it installed in your `PATH` for this to work).

this module only writes the h264 stream, the audio channels will need to be
combined manually, maybe like
```
 ffmpeg -i output.h264 -i audio.mp3 combined.mp4
```

## connectors

* `input` the sink to read the raw pixel data

## parameters

* `filename` the output filename to write the h264 stream to.
