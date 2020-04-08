#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "usage: noise-profile.sh <raw image file>"
    exit -1
fi
fname=$(./vkdt-cli -g examples/noiseprofile.cfg --config param:i-raw:01:filename:$1 2>&1| grep nprof | cut -d\' -f2)
mv $fname data/nprof/
./vkdt-cli -g examples/noisecheck.cfg --config param:i-raw:01:filename:$1

