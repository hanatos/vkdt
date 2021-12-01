# cnn denoise

this reads the weights of gmic's resnet from a texture and evaluates
the network.

the current implementation is an unoptimised super slow baseline version,
and it will run out of gpu memory all the time.

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
