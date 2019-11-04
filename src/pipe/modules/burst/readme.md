TODO: wronski et al 2019
hasinoff et al 2016


# alignment:

* half.comp: mosaiced images -> 2x2 or 3x3 downsampling to get grey images (see demosaic half size)

* TODO.comp: downsample 4x 4x 4x (see laplacian reduce daisy chained?)

+-4x4 nbhood search on 8x8 tiles
+-4x4 nbhood search on 16x16 tiles
+-4x4 nbhood search on 16x16 tiles
+-1x1 nbhood search on 16x16 tiles

increase tile size with noise (we're not paying much for the tile size, only
for the search window)

how to find matching tiles: see nlmeans in darktable classic:

* dist.comp  : compute abs difference: image A and offset image B(x+u, y+v)
* shared/blur: blur result (box? maybe not needed..) support is tile size
* merge.comp : pixel x,y is difference A B x,y with delta u,v, write to match image (u,v,d)

best match rgb image: offset (u,v) + score (d)

for all offsets in search window: repeat the above (potentially scheduled in
parallel) and
* merge best match image (read both and output merge)

XXX TODO: scheduling vs memory allocator? check whether it would be possible to free
GPU buffers in one parallel strand reused by the other without sync:

```

*---*---*---*--.
 `--*---*---*--*--

```

our allocator will finish one strand (dependency chain in graph traversal)
before considering the other, so the second strand might overwrite stuff
still in use by the other. not sure this is even executed in parallel though.



sub-pixel alignment by some iterations Lucas-Kanade [1981]
or distance field quadratic fitting


# merging:

TAA style failure detection:

compute spatial variance sigma and colour difference d
if d > alpha * sigma, discard. soft threshold:

R = s * exp( - d^2/sigma^2) - t

where s and t need to be tuned.
d and sigma computed from 2x2 downsampled ref and merged images (sigma in a 3x3 nbhood).

see their eq. (6)

