# physarum: mesmerizing interactive particle simulation

this module uses [the glsl shaders from Bleuje's github](https://github.com/Bleuje/interactive-physarum)
under:
Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License

this is based on a simulation detailed in the paper
**Characteristics of Pattern Formation and Evolution in
Approximations of Physarum Transport Networks,**
Jeff Jones, [Artificial Life 16: 127–153 (2010)](https://doi.org/10.1162/artl.2010.16.2.16202)

and also there is [a nice video explaining the context, by
Sebastian Lague](https://www.youtube.com/watch?v=X-iSQQgOd1A)

![physarum](./physarum.jpg)

## parameters

* `back` the parameter space point index for the background
* `cursor` the parameter space point index for the cursor area
* `colour` which colour scheme to use
* `wave` interactive simulation allows to fire waves through the particle system

## connectors

* `output` the rendered image based on coloured particle locations
* `mask` optional: input a grey scale image to modulate the pheromone trails
