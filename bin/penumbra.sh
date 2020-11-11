
#ref
mkdir -p penumbra/ref
rm penumbra/ref/*.jpg

./vkdt-cli -g examples/penumbra.cfg  --filename ref --output main --config param:penumbra:01:samples:512
ffmpeg -y -i ref_%04d.jpg -pix_fmt yuv420p penumbra-ref-512.mp4

mv ref_* penumbra/ref
mv -f penumbra*ref*mp4 penumbra

# base
mkdir -p penumbra/base
rm penumbra/base/*.jpg

./vkdt-cli -g examples/penumbra.cfg --filename base --output main
ffmpeg -y -i base_%04d.jpg -pix_fmt yuv420p penumbra-base-4.mp4

mv base_* penumbra/base
mv -f penumbra*base*mp4 penumbra

#opt
mkdir -p penumbra/opt
rm penumbra/opt/*.jpg

./vkdt-cli -g examples/penumbra-anim.cfg --filename opt --output main --output hist

ffmpeg -y -i hist_%04d.jpg -pix_fmt yuv420p penumbra-hist-blur2.mp4
ffmpeg -y -i opt_%04d.jpg -pix_fmt yuv420p penumbra-opt-blur2.mp4

mv hist_* penumbra/opt
mv opt_* penumbra/opt
mv -f penumbra*hist*mp4 penumbra
mv -f penumbra*opt*mp4 penumbra

# webm
# cd penumbra
# ffmpeg -y -i ref/ref_%04d.jpg   -b:v 16M penumbra-ref-512.webm
# ffmpeg -y -i base/base_%04d.jpg -b:v 16M penumbra-base-4.webm
# ffmpeg -y -i opt/hist_%04d.jpg  -b:v 16M penumbra-hist-blur2.webm
# ffmpeg -y -i opt/opt_%04d.jpg   -b:v 16M penumbra-opt-blur2.webm