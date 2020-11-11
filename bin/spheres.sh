#!/bin/bash
mkdir -p spheres/

./vkdt-cli -g examples/spheres-anim.cfg --filename our --output main --filename mv --output hist

ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-mv.mp4
ffmpeg -y -i our_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-our.mp4

# input
./vkdt-cli -g examples/spheres-anim.cfg --filename input --output main --config param:blend:01:opacity:0.0
ffmpeg -y -i input_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-input.mp4

# fake reference, remember to set SAMPLES=large in spheres/main.comp
./vkdt-cli -g examples/spheres-anim.cfg --filename ref --output main --config param:blend:01:opacity:0.0
ffmpeg -y -i ref_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-ref.mp4

# motion vectors for different distances
./vkdt-cli -g examples/spheres-anim.cfg --filename d0 --output main --filename mv --output hist --config param:burst:01:distance:0
ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-mv-d0.mp4
./vkdt-cli -g examples/spheres-anim.cfg --filename d0 --output main --filename mv --output hist --config param:burst:01:distance:2
ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-mv-d2.mp4
./vkdt-cli -g examples/spheres-anim.cfg --filename d0 --output main --filename mv --output hist --config param:burst:01:distance:3
ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-mv-d3.mp4
./vkdt-cli -g examples/spheres-anim.cfg --filename d0 --output main --filename mv --output hist --config param:burst:01:distance:4
ffmpeg -y -i mv_%04d.jpg -pix_fmt yuv420p -b:v 16M spheres-mv-d4.mp4

rm mv_*.jpg
rm our_*.jpg
rm input_*.jpg
mv -f spheres-mv*.mp4 spheres
mv -f spheres-our.mp4 spheres
mv -f spheres-input.mp4 spheres
