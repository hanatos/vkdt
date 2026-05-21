# reinhard: tone mapping for hdr input

## parameters

* `max_w` maximum white value

## connectors

* `input` scene referred and debayered data, works in-place or before a display transform
* `output` histogram compressed image data

## technical

this is mostly a port of the reinhard_extended_luminance function by 
Matt Taylor found at https://64.github.io/tonemapping/#extended-reinhard-luminance-tone-map
