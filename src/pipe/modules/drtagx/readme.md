# drtagx: agx display rendering transorm

this module applies a the AgX display rendering transorm 
that transorm the image data from scene referred for 
presentation on a display device

## parameters


## connectors

* `input`
* `output`

## technical

this is based on [three.js implementation](https://github.com/mrdoob/three.js/pull/27366/files), which is in turn based on Blender's implementation for rec 2020 colors as well as [iolite](https://iolite-engine.com/blog_posts/minimal_agx_implementation) and [filament](https://github.com/mrdoob/three.js/pull/27366), which are all an implementation of the original [AgX by Troy James Sobotka](https://github.com/sobotka/AgX). there is an attempt to implement a parameterized curve like that in [godot](https://github.com/godotengine/godot/blob/master/drivers/gles3/shaders/tonemap_inc.glsl)
