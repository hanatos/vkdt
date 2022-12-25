# command line interface (cli)

this initialises a headless vulkan compute shader pipeline, i.e. it does
not require an x server to be run.

```
usage: vkdt-cli -g <graph.cfg>
    [-d verbosity]                set log verbosity (none,qvk,pipe,gui,db,cli,snd,perf,mem,err,all)
    [--last-frame-only]           only write the last frame, not the intermediates
    [--dump-modules|--dump-nodes] write graphvis dot files to stdout
    [--quality <0-100>]           jpg output quality
    [--width <x>]                 max output width
    [--height <y>]                max output height
    [--filename <f>]              output filename (without extension or frame number)
    [--format <fm>]               output format (o-jpg, o-bc1, o-pfm, ..)
    [--output <inst>]             name the instance of the output to write (can use multiple)
                                  this resets output specific options: quality, width, height, audio
    [--audio <file>]              dump audio stream to this file, if any
    [--device <gpu name>]         explicitly use this gpu if you have multiple
    [--config]                    everything after this will be interpreted as additional cfg lines
```
