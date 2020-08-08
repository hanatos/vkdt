# colour picker

attach this module to the output of any other module. it will collect
statistics for the box you selected in the gui. it is possible to collect up to
20 boxes simultaneously (after that you'll need to attach another instance of
this module).

the picked colour will appear as parameter of this module, so you'll be able
to copy/paste it from the `.cfg` file afterwards.

note that the gui will only instant-update the picked colour if the runflags
for the graph contain *download sink*.
