# o-copy: simply copy the main input file to a new destination

this is useful if you want to relocate only parts of a folder, say if you need
to archive it, hand over to a client, or want to localise tagged collections
to a new folder. these copies can then be moved to mass storage or be edited
as truely independent projects: tagged collections on the other hand will only
be links to the original files and thus edits will change the look of the image
on the original folder too. such individually edited collections are useful
if you want to give a calendar a consistent look (aspect ratio..?) or want to
prepare training data for neural networks.

a second use case is to archive a project with all dependencies. while the cfg
sidecar file contains all information about the graph, there may be other files
the project depends on: colour lookup tables, weights for neural networks, or
secondary images when blending together multiple. selecting `bundle all external files`
in the `copy` option will let you track and collect these dependencies, too.

## connectors

* `input` this data will all be ignored and discarded

## parameters

* `filename` the filename to be written to disk. the extension will be preserved from the input.
* `copy` only export and rename the main source (i.e. raw file) or bundle a project with external dependencies such as luts and network weights
