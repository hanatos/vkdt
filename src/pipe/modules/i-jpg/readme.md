# i-jpg: jpeg image input

this module reads jpeg from the input. to make it useful, you may want to
include a [`resize` module](../resize/readme.md) to correctly scale the image
for thumbnails, and definitely a [`srgb2f` module](../srgb2f/readme.md) to
bringt it into linear rec2020.

## connectors

* `output`: the 8-bit rgba srgb data read from the jpeg image.
