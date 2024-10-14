# o-copy: simply copy the main input file to a new destination

this is useful if you want to relocate only parts of a folder, say if you need
to archive it, hand over to a client, or want to localise tagged collections
to a new folder. these copies can then be moved to mass storage or be edited
as truely independent projects: tagged collections on the other hand will only
be links to the original files and thus edits will change the look of the image
on the original folder too. such individually edited collections are useful
if you want to give a calendar a consistent look (aspect ratio..?) or want to
prepare a bunch of data is training data for neural networks.

## connectors

* `input` this data will all be ignored and discarded

## parameters

* `filename` : the filename to be written to disk. the extension will be preserved from the input.
