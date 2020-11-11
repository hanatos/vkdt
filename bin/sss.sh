#!/bin/bash

mkdir -p sss/

# ref
./vkdt-cli -g examples/sss-anim.cfg  --filename ref --output main --config param:sss:01:spp:10000 param:blend:01:opacity:0
ffmpeg -y -i ref_%04d.jpg -pix_fmt yuv420p -b:v 16M sss-ref-10k.mp4

rm ref_*.jpg
mv -f sss-ref-10k.mp4 sss

# base
./vkdt-cli -g examples/sss-anim.cfg --filename base --output main --config param:blend:01:opacity:0
ffmpeg -y -i base_%04d.jpg -pix_fmt yuv420p -b:v 16M sss-base-50.mp4

rm base_*.jpg
mv -f sss-base-50.mp4 sss

# opt
./vkdt-cli -g examples/sss-anim.cfg --filename opt --output main --filename mv --output hist

ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M sss-mv.mp4
ffmpeg -y -i opt_%04d.jpg -pix_fmt yuv420p -b:v 16M sss-opt.mp4

rm mv_*.jpg
rm opt_*.jpg
mv -f sss-mv.mp4 sss
mv -f sss-opt.mp4 sss
