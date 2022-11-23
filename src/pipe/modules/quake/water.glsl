// fast procedural water ray marching
// afl_ext 2017-2019
// https://www.shadertoy.com/view/MdXyzX

#define WATER_DRAG_MULT 0.048
#define WATER_IT 13
#define WATER_IN 48

// return heightfield of waves
float water_height(vec2 position, int iterations)
{
  position *= 0.005;
  float iter = 0.0;
  float phase = 6.0;
  float speed = 0.02;
  float weight = 1.0;
  float w = 0.0;
  float ws = 0.0;
  for(int i=0;i<iterations;i++)
  {
    vec2 p = vec2(sin(iter), cos(iter));
    float x = dot(p, position) * phase + global.frame * speed;
    float wave = exp(sin(x) - 1.0);
    float dx = wave * cos(x);
    vec2 res = vec2(wave, -dx);
    position += p * res.y * weight * WATER_DRAG_MULT;
    w += res.x * weight;
    iter += 12.0;
    ws += weight;
    weight = mix(weight, 0.0, 0.2);
    phase *= 1.18;
    speed *= 1.07;
  }
  return w / ws;
  // bias a bit more towards 1.0
  return 0.5*(1.0 + w / ws + 0.75);
}

float // return distance to camera // TODO: do we need it?
water_intersect(
    vec3  pos,   // entry point into geo
    vec3  dir,   // ray direction
    float depth) // depth of wave layer
{
  // now it becomes interesting. when looking from below, the surface should be *closer* than
  // the geometry entry point for symmetry. we accept that near borders this will not work correctly.
  bool reverse = dir.z > 0; // reverse means we're looking from below
  if(reverse) dir = -dir;
  pos.z = depth;
  float t = 0.0;
  const float sh = 0.001 + sqrt(1.0-dir.z*dir.z)*0.8;
  for(int i=0;i<318;i++)
  {
    float h = water_height(pos.xy, WATER_IT) * depth;
    if(h + 0.005 > pos.z) return reverse ? -t : t;
    // float s = max(0.005, pos.z - h);
    float s = clamp(sh*(pos.z - h), 0.001, 10);
    pos += dir * s;
    t += s;
  }
  return 10000.0;
}

vec4 // returns normal of wave pattern (more detailed than surface) and h in the w channel
water_normal(
    vec3  pos,   // intersection position
    float depth) // depth of wave layer
{ 
  vec2 posx = vec2(pos.x+1e-4, pos.y), posy = vec2(pos.x, pos.y+1e-4);
  float H = water_height(pos.xy, WATER_IN) * depth;
  vec3  a = vec3(pos.xy, H);
  return vec4(normalize(cross(
        (a-vec3(posx, water_height(posx, WATER_IN) * depth)),
        (a-vec3(posy, water_height(posy, WATER_IN) * depth)))), H);
}
