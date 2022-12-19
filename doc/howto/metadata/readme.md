# how to customise metadata display

the metadata is sourced externally via the `exiftool` command.
so first make sure you installed it on your system. the exact
command that is executed to create the view in the `metadata` expander
in lighttable mode is defined in the `~/.config/vkdt/config.rc` file:

```
strgui/metadata/command:/usr/bin/exiftool -l -createdate -aperture -shutterspeed -iso
```

this default command will print in two line format (`-l`) the information
that follows after it. consult `man exiftool` for more options.
