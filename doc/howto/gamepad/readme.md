# how to use your gamepad

vkdt supports some basic interaction via gamepad, such as navigating
around lighttable and darkroom modes as well as applying star ratings.

currently, best tested is a sony dualshock controller. to map buttons and axes
to ui interactions, the sdl gamecontrollerdb is used. vkdt will attempt to load
updated descriptions upon startup, from `/usr/share/sdl/gamecontrollerdb.txt`.
vkdt will skip joystick devices that do not have a gamepad mapping. this rules
out most unsuitable devices such as special keyboards or certain mouse pads.
still, the use of joysticks is switched off by default. to enable it, make sure
your `~/.config/vkdt/config.rc` contains a line:

```
intgui/disable_joystick:0
```

in general, `x` selects (enters lighttable/darkroom) and `o` aborts (goes back
to lighttable/files view).

to bring up context sensitive help overlaid on your screen, press the
playstation button in the center:

<video width="100%" controls>
<source src="gamepad.webm" type="video/webm">
no video
</video>
