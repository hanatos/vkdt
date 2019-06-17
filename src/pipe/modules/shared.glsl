struct roi_t
{
  uvec2 full;      // full input size
  uvec2 ctx;       // size of downscaled context buffer
  uvec2 roi;       // dimensions of region of interest
  ivec2 off;       // offset in full image
  float roi_scale; // scale: roi_wd * roi_scale is on input scale
  float pad0;      // alignment of structs will be a multiple of vec4 it seems :(
  float pad1;      // so we pad explicitly for sanity of mind.
  float pad2;      // alternatively we could specify layout(offset=48) etc below.
};


