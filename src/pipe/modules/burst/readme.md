# fast temporal reprojection without motion vectors

this is the source code as described in the corresponding paper.

to reproduce the subsampled figures, there is a place in the code
that needs adjustment and recompilation:

* `modules/burst/config.h` this define controls the 2x2 downsampling

these settings can be changed run-time as module parameters:

* `blend boxclamp`: enable or disable TAA-style box clamping

* `blend opacity`: if you set this to 0, you will see the single-frame input

* `burst mv_scale`: scale the motion vectors before converting to a colour

* `burst distance`: different distance formulas. from 0 to 4: log L1, log L1 center weighted, L1, L2, L2 center weighted


## run

from within the `bin/` directory:

`./vkdt examples/penumbra-anim.cfg`

`./vkdt examples/spheres-anim.cfg`

`./vkdt examples/sss-anim.cfg`


create helper noise textures:

`./vkdt-cli -g examples/himalaya-noise2.cfg --filename examples/hnoise2 --output main`

`./vkdt-cli -g examples/himalaya-noise3.cfg --filename examples/hnoise3 --output main`

run the himalaya scene:

`./vkdt examples/himalaya-anim.cfg`
