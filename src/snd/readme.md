# sound

the only audio backend in place for now is using *alsa* directly.
to run on a particular pcm, you can alter the `strsnd/alsa/pcm`
setting in `~/.config/vkdt/config.rc`. if you're running pipewire
for instance, include this line:
```
strsnd/alsa/pcm:pipewire
```
