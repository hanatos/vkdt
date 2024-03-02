// abstraction for feeding input to the MLP (used during inferencing and backpropagation)
float // return channel
load_input_tap(
    sampler2D img,
    uint      pxid,   // pixel coordinate of center pixel
    uint      chan,   // channel
    vec2      noise)  // gaussian and poissonian noise parameters
{
  const uint stride = textureSize(img, 0).x/2;
  ivec2 px = 2*ivec2(pxid % stride, pxid / stride);
#if WIDTH==64
  uint j = chan / 8;
  uint i = chan - 8*j;
  px += ivec2(i-3, j-3);
  if(px.x < 0) px.x = -2*((px.x+1)/2) + (px.x & 1);
  if(px.x >= textureSize(img, 0).x) px.x = 2*(textureSize(img, 0).x - px.x/2 - 1) + (px.x & 1);
  if(px.y < 0) px.y = -2*((px.y+1)/2) + (px.y & 1);
  if(px.y >= textureSize(img, 0).y) px.y = 2*(textureSize(img, 0).y - px.y/2 - 1) + (px.y & 1);
  return texelFetch(img, px, 0).r;
#else
#error demosaicing needs 64 wide networks!
#endif
}
