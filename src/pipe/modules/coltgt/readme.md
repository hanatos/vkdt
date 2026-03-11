# coltgt: colour target printing a few patches for display colour management

this module outputs the ColorChecker24 patches in bt2020, plus an additional
row with fully saturated bt2020 primaries as well as the mixed coordinates for
magenta, yellow, and cyan.

this can be useful in conjunction with say argyll's spotread to verify a
display colour management setup.

the bottom two rows contain a stripe pattern with 50% grey and 50% fully lit
pixels, as well as an average 0.5 row. if your monitor gamma is set correctly
and the colour profile is correct, these two should look the same if viewed
at 100% zoom from some distance. sometimes it's necessary to move the window
around a bit to avoid resampling issues.

![colour target](coltgt.jpg)
