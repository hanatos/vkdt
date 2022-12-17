# how to process timelapses with keyframes

`vkdt` natively supports animations (see the `esoteric`->`animation` expander
in darkroom mode). it is therefore straight forward to process timelapses.

## load an image sequence

to wire consecutive images in a timelapse sequence to individual frame numbers
in an animation, use
[the `i-raw` module](../../../src/pipe/modules/i-raw/readme.md) for raw images or
[the `i-jpg` module](../../../src/pipe/modules/i-jpg/readme.md) for jpg images.
they both support loading animations in the following way: you should set the
`filename` parameter to say `IMG_%04d.CR2` and the `startid` parameter to the
first image in the sequence (say 305 if `IMG_0305.CR2` is the filename of the
first frame). the module will then construct a filename for every frame,
starting at `startid` and using the total frame count set on the graph. you can
also set the `fps` global parameter in the `.cfg` file to control playback
speed (also accessible in the `esoteric`→`animation` expander in the gui).
`vkdt` will drop frames if it cannot decode the files from disk fast enough.

to see whether that worked, press `space` to play the animation (or use the
`play` button in the `animation` expander in the gui). pressing `space` again
will pause and `backspace` will reset to the first frame.


## using keyframes

note that the procedure described here will also work for single image
animations (say you want to zoom/rotate it or fade it in), video input (ldr or
raw), or rendered animations.

to create a keyframe, make sure the animation is set to the correct frame and
paused. then hover over the control of the parameter you wish to create a
keyframe for. this also works with keyboard/gamepad navigation.

then create a keyframe by pressing the associated hotkey (default `ctrl-k`, you
can adjust it to your liking in the `settings`→`hotkeys` dialog).

to test whether this worked, you will want to create at least a second
keyframe: navigate to the other frame, change the parameter of the same control
to something else, keep hovering over the control, press `ctrl-k` again.

if you play the animation from the start now (press `backspace` followed by
`space`) you should see an effect. a good first test could be the rotation
parameter of the `crop` module.


## exporting animations

there are a few options to export animated content. you can either use a
sequence of individual images by using say the
[`o-jpg` module](../../../src/pipe/modules/o-jpg/readme.md). you can also directly
[write video output](../../../src/pipe/modules/o-ffmpeg/readme.md).
note that this will output the h264 video stream and the audio separately
in a different file, it is currently necessary to combine them in a container
after export.
