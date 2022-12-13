# how to use your gamepad

currently, best tested is a sony dualshock controller.

to avoid side effects especially with certain mouse pads being detected as joysticks,
the use of joysticks is switched off by default. to enable it, make sure your
`~/.config/vkdt/config.rc` contains a line:

```
intgui/disable_joystick:0
```

in general, `x` selects (enters lighttable/darkroom) and `o` aborts (goes back
to lighttable/files view).

to bring up context sensitive help overlaid on your screen, press the
playstation button in the center.
