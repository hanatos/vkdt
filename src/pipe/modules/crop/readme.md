# crop, rotate, and perspective correction

this module allows you to crop and rotate an image. it also features a
perspective correction button, which will let you pick a quad on the image and
make these lines parallel after application.

# parameters

* `perspect` the quad used for perspective correction
* `crop` the axis aligned quad used for cropping
* `rotate` the rotation angle in degrees. if it is set to the special value
  `1337`, it will pick the rotation angle from the orientation exif metadata if
  it is passed along with the input image buffer. this is also the default.
