# f2srgb: convert linear rec2020 to srgb

that.

## connectors

* `input`
* `output`

## parameters

* `usemat` use the rec2020 to srgb matrix if this is set to 1 (the default).

this parameter is a backdoor for thumbnail generation. for performance reasons,
these are encoded as rec2020, but for better encoding in
[bc1](../i-bc1/readme.md) they do use the srgb trc.
