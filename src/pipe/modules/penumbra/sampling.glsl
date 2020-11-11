// tiny encryption algorithm random numbers
vec2 encrypt_tea(uvec2 arg)
{
  const uint key[] = {
    0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e
  };
  uint v0 = arg[0], v1 = arg[1];
  uint sum = 0;
  uint delta = 0x9e3779b9;

  #pragma unroll
  for(int i = 0; i < 16; i++) {
    sum += delta;
    v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
    v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
  }
  return vec2(v0/(0xffffffffu+1.0), v1/(0xffffffffu+1.0));
}

vec2 next_rand(inout uvec2 seed)
{
  seed.y++;
  return encrypt_tea(seed);
}

// uniform hemisphere sampling
vec3 sample_hemisphere_uniform(const vec2 rand)
{
    float sinT = fsqrt(1.0 - rand.x*rand.x);
    float phi = 2.0*M_PI*rand.y;
    vec3 v = vec3(sinT*cos(phi), sinT*sin(phi), rand.x);

    return v;
}

float pdf_hemisphere_uniform()
{
    return 1.0/(2.0*M_PI);
}

// uniform sphere sampling
vec3 sample_sphere_uniform(const vec2 rand)
{
    // uniform L on sphere
    float cos_th = 1.0 - 2.0*rand.x, sin_th = fsqrt(1.0 - cos_th*cos_th);
    float phi = 2.0*M_PI*rand.y;
    const vec3 v = vec3(cos(phi) * sin_th, sin(phi) * sin_th, cos_th);

    return v;
}

float pdf_sphere_uniform()
{
    return 1.0/(4.0*M_PI);
}