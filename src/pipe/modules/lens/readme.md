# lens: apply or correct lens distortions

this implements a very simple distortion correction following
[The Double Sphere Camera Model Vladyslav Usenko, Nikolaus Demmel and Daniel Cremers](https://arxiv.org/pdf/1807.08957.pdf).

# parameters

* `center` move the center of the projection
* `scale` scale the image larger or smaller
* `squish0` the radius of one of the spheres
* `squish1` the other radius used for the mapping
* `ca` some fake chromatic aberration correction, just scales differently for the rgb channels
