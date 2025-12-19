# agx: agx display rendering transform

this module applies a the AgX display rendering transform 
along with the allenwp tonemapping curve to transform the 
image data from scene referred to display referred for 
presentation on a display device.

## parameters


## connectors

* `input`
* `output`

## technical

this is based on three.js implementation[1], which is in turn based on 
Blender's implementation for rec 2020 colors as well as iolite[2], filament[3], 
godot[4], and AgX_LUT_Gen[5] which are all an implementation of the original AgX by Troy James Sobotka[6]. 
this also uses allenwp tonemapping curve[7] like that in godot[8]

- [1] https://github.com/mrdoob/three.js/pull/27366
- [2] https://iolite-engine.com/blog_posts/minimal_agx_implementation
- [3] https://github.com/google/filament/pull/7236
- [4] https://github.com/godotengine/godot/blob/master/drivers/gles3/shaders/tonemap_inc.glsl
- [5] https://github.com/EaryChow/AgX_LUT_Gen/blob/main/AgXBaseRec2020.py
- [6] https://github.com/sobotka/AgX
- [7] https://allenwp.com/blog/2025/05/29/allenwp-tonemapping-curve/
- [8] https://github.com/godotengine/godot-proposals/issues/12317#issuecomment-2835824250