# draw: raster masks as brush strokes

this module rasterises a drawn mask (single opacity channel). to be useful, it
has to be combined with blending. quick access is possible by adding the `draw`
block template to the graph (see `data/blocks/draw.cfg` and the *insert block..*
button in the pipeline configuration tab in darkroom mode).

for the impatient, there are also two examples in the repository:
`examples/draw.cfg` and `examples/draw-guided.cfg`. be sure to copy a
`test.cr2` file into the `examples/` directory before testing.

this block also contains a guided filter blur for edge-adaptive smoothing of
the resulting mask.

the module allows to draw line segments by clicking and holding the left mouse
button while drawing a stroke. releasing the button will finalise the stroke.
to remove the last stroke, press the right mouse button.
to draw a straight line, hold shift and click the start and then the end point.

you can also alter the properties of the strokes: opacity, radius, and
hardness. these properties can be set globally as parameters on sliders, and will
change the look of all the strokes in this instance even after they have been drawn.
to vary the individual strokes, you can change the properties per vertex using
the scroll wheel:

* scrolling changes the radius per vertex
* ctrl-scrolling changes the hardness per vertex
* shift-scrolling changes the opacity per vertex.

these properties are then multiplied to the global settings changed via the sliders
for the final result. the per-vertex properties are indicated in the ui as two circles:
the outer circle is the radius, the inner indicates hardness. opacity is shown as a small
quad that is square for full opacity and very thin for low opacity.

*blending* is performed in such a way that the strokes add up opacity up until a maximum
value of what is set as `opacity` parameter (for instance in the gui with slider). this
way you can easily draw masks of level opacity, since the strokes merge seamlessly.

## connectors

* `output` the single channel floating point mask as drawn, connect this to a blend module
* `curve` the brush strokes as list of vertices for further geometric processing, for most use cases you can ignore this

## parameters

* `opacity` the global opacity of all brush strokes in this instance
* `radius` the global radius of all brush strokes in this instance, as fraction of image width
* `hardness` the global hardness of the brush strokes. 1.0 means no feathering
* `draw` the array of coordinates of the brush strokes. these are set in the gui by drawing with the mouse

## examples

the following images are a collection of brush strokes achieved with various
settings, and they also show the visual feedback of the mouse cursor.

the outer circle shows the size of the stroke (radius).  
![hard opaque](stroke-hard-opaque.jpg)

scrolling enlarges the radius.  
![large radius](stroke-hard-opaque-large.jpg)

shift-scrolling changes the opacity, shown by the *squareness* of the little quad.  
![hard transparent](stroke-hard-transparent.jpg)

hardness is indicated by the relative size of the second, inner circle.  
![soft opaque](stroke-soft-opaque.jpg)

it can be observed better for lower transparency.  
![very soft transparent](stroke-verysoft-transparent.jpg)

these examples also show how the strokes are blended together (via max) to seamlessly merge
to 100%.


the [guided filter](../guided/readme.md) module can be used to make the brush stroke
adapt to edges in the image, and is present in the default draw block as mentioned
above. as an example, consider this brush stroke,
which is transformed by the guided filter to look like the left overlay (drag
the triangle at the bottom of the white line to compare):  
<div class="compare_box">
<textarea readonly style="background-image:url(guided-off.jpg)"></textarea>
<textarea readonly style="background-image:url(guided-on.jpg)" ></textarea>
</div>

in this setup, the guided filter and the draw module are wired the following way:

![wiring](wiring.jpg)

and the masked area has an additional `exposure` module applied before blending
back, in the spirit of dodging/burning. you can insert any other modules here, of course.
