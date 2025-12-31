# how to customise metadata display

the metadata is sourced externally via the `exiftool` command.
first make sure exiftool is installed on your system and the binary 
is in the system or user PATH. the flags passed to exiftool to create 
the view in the `metadata` expander in lighttable mode is defined in the 
`~/.config/vkdt/config.rc` file:

```
strgui/metadata/flags:-l -createdate -aperture -shutterspeed -iso
```

this default command will print in two line format (`-l`) the information
that follows after it. consult `man exiftool` for more options.
