# cnn: convolutional neural network denoise

this reads the weights of [gmic's resnet](https://gmic.eu/reference/denoise_cnn.html) from a texture and evaluates
the network.

the current implementation runs in small tiles (24x24) and has only
been tested on a slow nvidia 1650 GTX. a future implementation
should probably focus on the turing architecture or more recent ones.

to generate the required weights `data/cnn.lut`, you need `gmic-3.0.0` and
dump the network weights like so (all in this directory):

```
gmic ./gmic_denoise_cnn.gmz k[3] unserialize repeat '$!' 'l[$>]' o '{b}'.pfm endl done
clang -Wall -std=c11 -I ../../../ ingest.c -g -o ingest
./ingest
mv cnn.lut ${VKDT_BIN_DIR}/data/
```

assuming that your git repo `bin/` or system wide installed `/usr/lib/vkdt`
directory names are stored in the envvar `${VKDT_BIN_DIR}`.
