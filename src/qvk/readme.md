# quake vulkan layer

these are some convenience c style wrappers around vulkan, all stolen from
christoph schied's quake2 vkpt implementation (GPLv2 because quake2 was that).

it builds

```
libqvk.a
```
as well as
```
libqvkgui.a
```
to support the bits that link against SDL2 and friends.
