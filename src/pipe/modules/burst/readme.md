TODO: wronski et al 2019


# alignment:

mosaiced images -> 2x2 downsampling to get grey images

downsample 4x 4x 4x

+-4x4 nbhood search on 8x8 tiles
+-4x4 nbhood search on 16x16 tiles
+-4x4 nbhood search on 16x16 tiles
+-1x1 nbhood search on 16x16 tiles


sub-pixel alignment by some iterations Lucas-Kanade [1981]


# merging:

TAA style failure detection:

compute spatial variance sigma and colour difference d
if d > alpha * sigma, discard. soft threshold:

R = s * exp( - d^2/sigma^2) - t

where s and t need to be tuned.
d and sigma computed from 2x2 downsampled ref and merged images (sigma in a 3x3 nbhood).

see their eq. (6)

