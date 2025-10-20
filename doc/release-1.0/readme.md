# release notes for vkdt 1.0

these release notes contain the change log over the last release, vkdt 0.9.1.
there are quite a few new features, ui changes, and new modules to work with.

## image processing/rendering
* [spectral film simulation](../../src/pipe/modules/filmsim/readme.md)  
  thanks to arctic for crunching all the data and doing the research!  
  thanks to jedypod for adding the latest film/paper stock!  
  ![neg](../../src/pipe/modules/filmsim/neg_004.png)
  ![pos](../../src/pipe/modules/filmsim/print_004.png)
* vkdt-rendered logo (animated, looks best in hdr mode)  
  ![vid](../../favicon.png)
* [i-lut](../../src/pipe/modules/i-lut/readme.md) now supports lists of input files, useful for textures/3d rendering
* improved the [grain module](../../src/pipe/modules/grain/readme.md) with additional parameters for low frequency components
  [![grain](../../src/pipe/modules/grain/grain.jpg)](../../src/pipe/modules/grain/grain.jpg)
* refactored `quake` and `rt` modules to use a unified spectral renderer  
  ![rt](../../src/pipe/modules/rt/rt.jpg)
* an implementation of [mcpg](https://www.lalber.org/2025/04/markov-chain-path-guiding/) (Markov chain path guiding) for 3d rendering/quake  
  ![quake](../../src/pipe/modules/quake/q1.png)
* removed some old modules

## video and animation
* keyframe interpolation modes in right-click menu (ease-in, ease-out, ..)  
  ![keyframe](keyframe.png)
* keyframed brush strokes (for real time hand drawn text for instance)
* mcraw input supports trimming from left (and right as always)

## ui rendering
* [scalable msdf fonts](../howto/fonts/readme.md) for sharp fonts in graph editor at any zoom level  
![zoom msdf](zoom-msdf.png)
* hdr support, if you use wayland and the compositor does it, try  
  `ENABLE_HDR_WSI=1 vkdt`  
  (see [display colour docs](../howto/colour-display/readme.md) for details)
* filtered minification for display (improved grain/noise inspection)

## ui
* quick access preset buttons in customisable favourites tab
* knobs widget (see `frame` or `grade` module for an example)
  use [hsluv](https://www.hsluv.org/) as ui colour picker space  
  ![knobs](knobs.png)
* dynamic lod (option to render low res when dragging sliders/drawing masks)
* right click/gamepad-controlled radial widget for quick access to favourites  
  ![radial](radial-menu.png)
* introduce mouse interaction on `dspy` outputs of modules to change their parameters. 
  the new `curves` module is the first user.  
  ![curves](curves.png)
* lighttable: can filter filenames by regular expression now
* lighttable: allow moving selected files around on disk
* lighttable: access favourite preset buttons from here too
* backend time offset support for sorting images from multiple cameras
* re-introduced darkroom mode zoom/pan via [gamepad](../howto/gamepad/readme.md)
* many smaller ui fixes and polish, better custom `style.txt` support

## build and infrastructure
* introduce [core and beta modules](../core.md) for backward compatibility guarantees
* improved wayland support (better than x11)
* rewrote `vkdt read-icc` in c to reduce dependencies for packaging
* arch aur package `vkdt-git`
* android compile fixes (thanks paolo!)


### new modules
* [dehaze](../../src/pipe/modules/dehaze/readme.md) haze removal and depth estimator
* [filmsim](../../src/pipe/modules/filmsim/readme.md) spectral film simulation
* [curves](../../src/pipe/modules/curves/readme.md) per-channel rgb/luminance or 3x3 combinations of ych curves
* [o-copy](../../src/pipe/modules/o-copy/readme.md) to copy raw images on export
* [jddcnn](../../src/pipe/modules/jddcnn/readme.md) experimental joint demosaicing and denoising based on a CNN
* [g-csolid](../../src/pipe/modules/g-csolid/readme.md) convert drawn brush strokes to 3d geometry
* [bvh](../../src/pipe/modules/bvh/readme.md) 3d geometry sink for ray tracing
* [i-obj](../../src/pipe/modules/i-obj/readme.md) 3d geometry source
* [physarum](../../src/pipe/modules/physarum/readme.md) mesmerising slime mould particle sim
