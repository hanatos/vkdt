
#ref
mkdir -p himalaya/ref
rm himalaya/ref/*.jpg

./vkdt-cli -g examples/himalaya.cfg  --filename ref --output main --config param:himalaya:01:cmarchs:60 param:himalaya:01:msamples:16
ffmpeg -y -i ref_%04d.jpg -pix_fmt yuv420p himalaya-hq.mp4

mv ref_* himalaya/ref
mv -f himalaya*hq*mp4 himalaya

# base
mkdir -p himalaya/base
rm himalaya/base/*.jpg

./vkdt-cli -g examples/himalaya.cfg --filename base --output main
ffmpeg -y -i base_%04d.jpg -pix_fmt yuv420p himalaya-base.mp4

mv base_* himalaya/base
mv -f himalaya*base*mp4 himalaya

#opt
mkdir -p himalaya/opt
rm himalaya/opt/*.jpg

./vkdt-cli -g examples/himalaya-anim.cfg --filename opt --output main --config param:blend:01:opacity:0.93

ffmpeg -y -i opt_%04d.jpg -pix_fmt yuv420p himalaya-opt-blur2.mp4

mv opt_* himalaya/opt
mv -f himalaya*opt*mp4 himalaya
