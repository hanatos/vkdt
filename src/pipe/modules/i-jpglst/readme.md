# i-jpglst: jpeg array input

this node allows you to load a large array of images of possible different
resolution. it takes as argument a text file with one filename per line. the
output connector will be an array connector with the images tied to the
elements in the order as they appear in the file.

## parameters

* `filename` a plain text file with one texture filename per line

## connectors

* `output` a list of ui8 textures as an array connector
