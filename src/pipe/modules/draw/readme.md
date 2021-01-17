# draw raster masks

this module rasterises a drawn mask (single opacity channel). to be useful, it
has to be combined with blending. quick access is possible by adding the `draw`
block template to the graph (see `data/blocks/draw.cfg` and the *add block..*
button in the pipeline configuration tab in darkroom mode.

this block also contains a guided filter blur for edge-adaptive smoothing of
the resulting mask.

the module allows to draw line segments and alter their properties: opacity, radius, and
hardness. these properties can be set globally as parameters on sliders, and will
change the look of all the strokes in this instance even after they have been drawn.
to vary the individual strokes, you can change the properties per vertex using
the scroll wheel:

* scrolling changes the radius per vertex
* shift-scrolling changes the hardness per vertex
* ctrl-scrolling changes the opacity per vertex.

these properties are then multiplied to the global settings changed via the sliders
for the final result. the per-vertex properties are indicated in the ui as two circles:
the outer circle is the radius, the inner indicates hardness. opacity is shown as a small
quad that is square for full opacity and very thin for low opacity.

*blending* is performed in such a way that the strokes add up opacity up until a maximum
value of what is set as `opacity` parameter (for instance in the gui with slider). this
way you can easily draw masks of level opacity, since the strokes merge seamlessly.

## parameters

* `opacity` the global opacity of all brush strokes in this instance
* `radius` the global radius of all brush strokes in this instance, as fraction of image width
* `hardness` the global hardness of the brush strokes. 1.0 means no feathering
* `draw` the array of coordinates of the brush strokes. these are set in the gui by drawing with the mouse
