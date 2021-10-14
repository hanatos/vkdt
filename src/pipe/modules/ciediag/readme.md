# cie chromaticity diagram

this module draws a scatterplot of the input image on a cie chromaticity
diagram. this can be useful to visualise out of gamut/spectral locus colours and
to aim for a certain illuminant during white balancing.
the red cross indicates illuminant E (equal energy white, the white point of XYZ),
the green line is the planckian locus, i.e. black body emission chromaticities.

in the background, the gamuts of rec709 (the smaller triangle ) and rec2020
(the larger triangle) are indicated as light gray shade.
