// fast procedural water ray marching
// afl_ext 2017-2019
// https://www.shadertoy.com/view/MdXyzX

#define WATER_DRAG_MULT 0.1 // 0.048
#define WATER_IT 13//5//2 // 13
#define WATER_IN 15//7//3 // 15 //48 [jo] i think 48 is pretty much emulating microfacet models directly :)

// return heightfield of waves
float water_height(float cl_time, vec2 position, int iterations)
{
  // return 1.0;// XXX switch off
  position *= 0.005;
  float iter = 0.0;
  float phase = 6.0;
  float speed = 0.5;
  float weight = 1.0;
  float w = 0.0;
  float ws = 0.0;
  for(int i=0;i<iterations;i++)
  {
    vec2 p = vec2(sin(iter), cos(iter));
    float x = dot(p, position) * phase + cl_time * speed;
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
    float cl_time, // animation frame
    vec3  pos,     // entry point into geo
    vec3  dir,     // ray direction
    float depth,   // depth of wave layer
    bool  top)     // entry point is the top surface of the two (at h = depth, not the bottom at h = 0)
{
  // return 0.0;// XXX switch off
  // now it becomes interesting. when looking from below, the surface should be *closer* than
  // the geometry entry point for symmetry. we accept that near borders this will not work correctly.
  bool reverse = dir.z > 0; // reverse means we're looking from below
  pos.z = top ? depth : 0.0;
  // if dir.z > 0 but intersected the top surface (in the side stripe going up)
  // we need to walk backwards and return -t
  // since height h < pos.z, the stepsize pos.z - h will be negative and facilitate that
  // exit criterion:
  // (-) if pos.z < h //  reverse &&  top // side stripe looking up
  // (+) if pos.z < h // !reverse &&  top // easy normal case looking down into the water
  // (+) if pos.z > h //  reverse && !top // normal case under water looking up
  // (-) if pos.z > h // !reverse && !top // side stripe looking down
  // sign of ray march should always be + if reverse ^^ top
  float sgn = (reverse ^^ top) ? 1.0 : -1.0;
  float t = 0.0;
  const float sh = mix(0.5, 1.0, dir.z);
  for(int i=0;i<300;i++)
  {
    float h = water_height(cl_time, pos.xy, WATER_IT) * depth;
    if(!top && h - 0.005 < pos.z) return t;
    if( top && h + 0.005 > pos.z) return t;
    float s = sgn*clamp(sh*abs(pos.z - h), 0.001, 10);
    pos += dir * s;
    t += s;
  }
  return 10000.0;
}

vec4 // returns normal of wave pattern (more detailed than surface) and h in the w channel
water_normal(
    float cl_time, // animation time
    vec3  pos,     // intersection position
    float depth)   // depth of wave layer
{ 
  vec2 posx = vec2(pos.x+1e-4, pos.y), posy = vec2(pos.x, pos.y+1e-4);
  float H = water_height(cl_time, pos.xy, WATER_IN) * depth;
  vec3  a = vec3(pos.xy, H);
  return vec4(normalize(cross(
        (a-vec3(posx, water_height(cl_time, posx, WATER_IN) * depth)),
        (a-vec3(posy, water_height(cl_time, posy, WATER_IN) * depth)))), H);
}
