# frame: draw postcard frame and decor line

this module allows you to draw a decorative frame with a line of different colour inside it
around the image.

![example frame](frame.jpg)

## connectors

* `input` the input image
* `output` the output with the frame added

## parameters

* `border` colour of the frame around the image
* `line` colour of the line inside the frame
* `size` fractions of the image width to add as frame horizontally and vertically 
* `linewd` width of the line inside the frame: 0.0 means no line, 1.0 means full frame size assuming centered alignment
* `align` where to put the image within the frame: 0.0 means align to the top/left, 0.5 is center, 1.0 means bottom/right
* `linepos` where to draw the decor line: 0.0 means directly around the image, 1.0 means on the outer rim of the frame
