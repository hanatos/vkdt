// helper functions
int binom(inout uint seed, int n, float p)
{ // gaussian approximation, good for large n*p
  float u = n*p;
  float s = sqrt(n*p*(1.0-p));
  vec2 r = vec2(mrand(seed), mrand(seed));
  return max(0, int(u + s * warp_gaussian(r).x));
}

float envelope(float w)
{
  return smoothstep(380.0, 400.0, w)*(1.0-smoothstep(700.0, 730.0, w));
}

vec3 thorlabs_filters(float w)
{
  // this is what looks like it might be a good fit to the thorlabs filters in a plot.
  // the lines marked with XXX smooth out the yellow/magenta transition at 500nm such that the two
  // filters sum to one. as a result i can fit the neutral filter configuration for supra and portra
  // without using negative weights (!!).
  // makes me think that somewhere else in the pipeline there might be something off.
  float cyan    = 0.93*smoothstep(345.0, 380.0, w)*(1.0-smoothstep(545.0, 590.0, w))+0.7*smoothstep(775.0,810,w);
  // float magenta = 0.9*smoothstep(355.0, 380.0, w)*(1.0-smoothstep(475.0, 505.0, w));
  float magenta = 0.9*smoothstep(355.0, 380.0, w)*(1.0-smoothstep(475.0, 525.0, w)); // XXX
  if(w > 550.0)
    magenta = 0.9*smoothstep(595.0, 645.0, w);
  // float yellow  = 0.92*smoothstep(492.0, 542.0, w) + 0.2*(1.0-smoothstep(370, 390, w));
  float yellow  = 0.92*smoothstep(475.0, 525.0, w) + 0.2*(1.0-smoothstep(370, 390, w)); // XXX
  return vec3(cyan, magenta, yellow);
}

vec2 hash(in ivec2 p)  // this hash is not production ready, please
{                        // replace this by something better
  ivec2 n = p.x*ivec2(3,37) + p.y*ivec2(311,113); // 2D -> 1D
  // 1D hash by Hugo Elias
  n = (n << 13) ^ n;
  n = n * (n * n * 15731 + 789221) + 1376312589;
  return -1.0+2.0*vec2( n & ivec2(0x0fffffff))/float(0x0fffffff);
}

float noise(in vec2 p)
{
  ivec2 i = ivec2(floor(p));
  vec2 f = fract(p);

  // quintic interpolation
  vec2 u = f*f*f*(f*(f*6.0-15.0)+10.0);
  vec2 du = 30.0*f*f*(f*(f-2.0)+1.0);

  vec2 ga = hash(i + ivec2(0,0));
  vec2 gb = hash(i + ivec2(1,0));
  vec2 gc = hash(i + ivec2(0,1));
  vec2 gd = hash(i + ivec2(1,1));

  float va = dot(ga, f - vec2(0.0,0.0));
  float vb = dot(gb, f - vec2(1.0,0.0));
  float vc = dot(gc, f - vec2(0.0,1.0));
  float vd = dot(gd, f - vec2(1.0,1.0));

  return va + u.x*(vb-va) + u.y*(vc-va) + u.x*u.y*(va-vb-vc+vd);   // value
}

// separate density into three layers of grains (coarse, mid, fine) that sum up
// to the max density. lower density values are taken mostly by coarse grains.
// use perlin gradient noise / correlated to simulate the three layers (represents non-uniformity of grain counts/pixel)
// simulate a poisson distribution x3: are the grains turned?
// expectation should be =density, i.e. n*p + n*p + n*p = density for the three layers
vec3 add_grain(ivec2 ipos, vec3 density)
{
  int n_grains_per_pixel = 1000; // from artic's python. starts to look very good!
  int grain_non_uniformity = int((1.0-params.grain_uniformity)*n_grains_per_pixel);
  float grain_size = params.grain_size;
  float density_max = 3.3;
  vec3 res = vec3(0.0);
  uint seed = 123456789*ipos.x + 1333337*ipos.y;
  vec3 particle_scale_col = vec3(0.8, 1.0, 2.0);
  vec3 particle_scale_lay = vec3(2.5, 1.0, 0.5);
  for(int col=0;col<3;col++)
  {
    float np = density[col] / density_max; // expectation of normalised number of developed grains
    vec3 doff = vec3(0.10, 0.20, 0.7);
    for(int layer=0;layer<3;layer++)
    { // from coarse to fine layers
      float npl = min(doff[layer], np);
      np -= npl;
      float c = 1.0/(particle_scale_col[col] * particle_scale_lay[layer]);//pow(2, layer);
      vec2 tc = c/grain_size * 0.3*vec2(ipos + 1000*col);
      // int r = int(grain_non_uniformity*noise(tc));
      int r = int(grain_non_uniformity*c*noise(tc));
      int n = int(n_grains_per_pixel*c);
      float p = npl;// / float(n);
      // if(layer!=2) res[col] += density_max * npl; else
      // uint seed = n;//n_grains_per_pixel;
      // res[col] += density_max * (n+r)*p/float(n); // this is using the expected value of developed grains directly
      // res[col] += density_max * poisson(seed, (n+r)*p)/float(n); // simulates poisson, too bright
      // now simulate whether these grains actually turn:
      res[col] += density_max * binom(seed, (n+r), p)/float(n); // looks broken too
    }
  }
  return res;
}

const int s_sensitivity = 0;
const int s_dye_density = 1;
const int s_density_curve = 2;
float get_tcy(int type, int stock)
{
  const float s = textureSize(img_filmsim, 0).y;
  const int off = type;
  return (stock * 3 + off + 0.5)/s;
}

