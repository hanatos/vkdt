#include "shared.glsl"
#include "shared/dtucs.glsl"

layout(local_size_x = DT_LOCAL_SIZE_X, local_size_y = DT_LOCAL_SIZE_Y, local_size_z = 1) in;

layout(std140, set = 0, binding = 1) uniform params_t
{
  vec4  mul;               // camera white balance (r,g,b, exposure)
  mat3  cam_to_rec2020;    // camera matrix
  uvec4 N;                 // number of patches <= 24
  mat3  rbf_P;             // rbf matrix part
  vec4  rbf_c[24];         // rbf coefficients
  vec4  rbf_p[24];         // rbf positions
  float temp;              // colour temperature for wb 0:2856 1:6504
  uint  colour_mode;       // 0-matrix 1-clut
  float saturation;        // multiplier on chroma
  uint  pick_mode;         // what do we do with the colour picked input?
  uint  gamut_mode;        // 0 nothing, 1 spec locus, 2 rec2020, 3, rec709
  uint  primaries;         // see module.h
  uint  trc;
  float clip_highlights;   // pass highlights through or clip at minimum of rgb after processing
} params;

layout(push_constant, std140) uniform push_t
{
  int have_clut;
  int have_pick;
  int have_abney; // only set if we have both img_abney and img_spectra
} push;


layout( // input
    set = 1, binding = 0
) uniform sampler2D img_in;

layout( // output
    set = 1, binding = 1
) uniform writeonly image2D img_out;

layout( // if have_clut, the colour lookup table is here
    set = 1, binding = 2
) uniform sampler2D img_clut;

layout( // picked colour if any.
    set = 1, binding = 3
#ifdef HAVE_NO_ATOMICS
) uniform usampler2D img_pick;
#else
) uniform sampler2D img_pick;
#endif

layout( // if have_abney, this contains the hue constancy map
    set = 1, binding = 4
) uniform sampler2D img_abney;

layout( // if have_abney, this contains the spectral upsampling table
    set = 1, binding = 5
) uniform sampler2D img_spectra;


vec3 // return adapted rec2020
cat16(vec3 rec2020_d65, vec3 rec2020_src, vec3 rec2020_dst)
{
  // these are the CAT16 M^{-1} and M matrices.
  // we use the standalone adaptation as proposed in
  // Smet and Ma, "Some concerns regarding the CAT16 chromatic adaptation transform",
  // Color Res Appl. 2020;45:172–177.
  // these are XYZ to cone-like
  const mat3 M16i = transpose(mat3(
       1.86206786, -1.01125463,  0.14918677,
       0.38752654,  0.62144744, -0.00897398,
      -0.01584150, -0.03412294,  1.04996444));
  const mat3 M16 = transpose(mat3(
       0.401288, 0.650173, -0.051461,
      -0.250268, 1.204414,  0.045854,
      -0.002079, 0.048952,  0.953127));
  const mat3 rec2020_to_xyz = mat3(
    6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
    1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
    1.68880975e-01, 5.93017165e-02, 1.06098506e+00);

  const vec3 cl_src = M16 * rec2020_to_xyz * rec2020_src;
  const vec3 cl_dst = M16 * rec2020_to_xyz * rec2020_dst;
  vec3 cl = M16 * rec2020_to_xyz * rec2020_d65;
  cl *= cl_dst / cl_src;
  return inverse(rec2020_to_xyz) * M16i * cl;
}


float
rbfk(vec3 ci, vec3 p)
{ // thinplate spline kernel
  float r2 = dot(ci-p, ci-p);
  return sqrt(r2);
  // if(r2 < 1e-8) return 0.0;
  // return r2 * sqrt(r2);
  // return r2 * log(r2);
}

void tri2quad(inout vec2 tc)
{
  tc.y = tc.y / (1.0-tc.x);
  tc.x = (1.0-tc.x)*(1.0-tc.x);
}

vec3 process_clut(vec3 rgb)
{
  float b = rgb.r+rgb.g+rgb.b;
  vec2 tc = rgb.rb/b;
  tri2quad(tc);
  tc.x /= 3.0;
  vec4 rbrb = vec4(texture(img_clut, tc).xy, texture(img_clut, tc+vec2(2.0/3.0, 0.0)).xy);
  vec2 L2 = texture(img_clut, tc + vec2(1.0/3.0, 0.0)).xy;
  float L = mix(L2.x, L2.y, params.temp);
  vec2 rb = mix(rbrb.xy, rbrb.zw, params.temp);
  rgb = vec3(rb.x, 1.0-rb.x-rb.y, rb.y);
  return rgb * L * b;
}

vec3 decode_colour(vec3 rgb)
{
  // if(params.trc == 0) // linear
  if(params.trc == 1) // 709
  {
    const float a = 1.09929682680944;
    const float b = 0.018053968510807;
    rgb = mix(rgb/4.5, pow((rgb+(a-1))/a, vec3(2.2)), greaterThan(rgb, vec3(b*4.5)));
  }
  else if(params.trc == 2) // sRGB
  { // undo tone curve
    rgb = mix(rgb/12.92, pow((rgb+0.055)/1.055, vec3(2.4)), greaterThan(rgb, vec3(0.04045)));
  }
  else if(params.trc == 3) // PQ
  { // eotf
    const float m1 = 1305.0/8192.0;
    const float m2 = 2523.0/32.0;
    const float c1 = 107.0/128.0;
    const float c2 = 2413.0/128.0;
    const float c3 = 2392.0/128.0;
    const vec3 xpow = pow(max(vec3(0.0), rgb), vec3(1.0/m2));
    const vec3 num = max(xpow - c1, vec3(0.0));
    const vec3 den = max(c2 - c3 * xpow, 1e-10);
    rgb = pow(num/den, vec3(1.0/m1));
  }
  else if(params.trc == 4) // DCI
  {
    rgb = pow(rgb, vec3(2.6));
  }
  else if(params.trc == 5) // HLG
  {
    const float a = 0.17883277, b = /*1.0 - 4.0*a =*/ 0.28466892, c = /*0.5 - a*log(4.0*a) =*/ 0.55991073;
    // zscale says:
    rgb = mix((exp((rgb-c)/a) + b)/12.0, rgb*rgb/3.0, lessThanEqual(rgb, vec3(0.5)));
  }
  else if(params.trc == 6) // gamma
  { // TODO get gamma from params
    rgb = pow(max(rgb, vec3(0.0)), vec3(2.2)); // happens to be adobe rgb
  }
  else if(params.trc == 7) // mcraw log to linear
  {
    const float a = 0.13, b = 1.0 - 4.0 * a, c = 0.5 - a * log(4.0 * a);
    rgb = mix(rgb * rgb / 3.0, (exp((rgb - c) / a) + b) / 12.0, greaterThan(rgb, vec3(0.5)));
  }

  if(params.primaries == 0)
  { // use uploaded custom matrix
    rgb = params.cam_to_rec2020 * rgb;
  }
  else if(params.primaries == 1) // srgb
  { // convert linear rec709 to linear rec2020
    const mat3 M = mat3(
      0.62750375, 0.06910828, 0.01639406,
      0.32927542, 0.91951916, 0.08801125,
      0.04330266, 0.0113596 , 0.89538035);
    rgb = M * rgb;
  }
  // else if(params.primaries == 2) // 2020 and 2100
  // { // identity 
  // }
  else if(params.primaries == 3) // adobeRGB
  {
    const mat3 rec2020_to_adobeRGB = mat3(
        0.87736306, 0.0966218 , 0.02291617,
        0.07751751, 0.89152263, 0.04301452,
        0.04516292, 0.01186405, 0.93367996);
    rgb = rec2020_to_adobeRGB * rgb;
  }
  else if(params.primaries == 4) // P3
  {
    const mat3 rec2020_to_display_P3 = mat3(
        0.75386031,  0.04575344, -0.00121501,
        0.19861268,  0.94178472,  0.01760596,
        0.04757049,  0.01247032,  0.98321971);
    rgb = rec2020_to_display_P3 * rgb;
  }
  else if(params.primaries == 5) // XYZ
  {
    const mat3 xyz_to_rec2020 = mat3(
      1.71665119, -0.66668435,  0.01763986,
     -0.35567078,  1.61648124, -0.04277061,
     -0.25336628,  0.01576855,  0.94210312);
    rgb = xyz_to_rec2020 * rgb;
  }
  return rgb;
}

void
main()
{
  ivec2 ipos = ivec2(gl_GlobalInvocationID);
  if(any(greaterThanEqual(ipos, imageSize(img_out)))) return;

  vec3 rgb = texelFetch(img_in, ipos, 0).rgb; // read camera rgb
  float cam_lum = 1.5;
  vec3 picked_rgb = vec3(0.5);
  if(push.have_pick == 1 && params.pick_mode > 0)
  {
    const int i = 0; // which one of the colour pickers
    picked_rgb = vec3(
#ifdef HAVE_NO_ATOMICS
        uintBitsToFloat(texelFetch(img_pick, ivec2(i, 0), 0).r),
        uintBitsToFloat(texelFetch(img_pick, ivec2(i, 1), 0).r),
        uintBitsToFloat(texelFetch(img_pick, ivec2(i, 2), 0).r));
#else
        texelFetch(img_pick, ivec2(i, 0), 0).r,
        texelFetch(img_pick, ivec2(i, 1), 0).r,
        texelFetch(img_pick, ivec2(i, 2), 0).r);
#endif
    cam_lum = dot(vec3(1), picked_rgb);
  }

  // process camera rgb to rec2020:
  if(params.colour_mode == 0 || push.have_clut == 0)
    rgb = decode_colour(rgb);
  else rgb = process_clut(rgb);

  if(push.have_pick == 1 && (params.pick_mode & 1) != 0)
  { // spot wb
    if(params.colour_mode == 0 || push.have_clut == 0)
      picked_rgb = decode_colour(picked_rgb);
    else
      picked_rgb = process_clut(picked_rgb);
    picked_rgb /= picked_rgb.g;
    rgb = cat16(rgb, picked_rgb, params.mul.rgb);
  } // regular white balancing
  else rgb = cat16(rgb, vec3(1.0), params.mul.rgb);

  if(params.clip_highlights > 0.0)
  {
    vec3 clip = vec3(params.clip_highlights);
    if(params.colour_mode == 0 || push.have_clut == 0)
      clip = decode_colour(clip);
    else rgb = process_clut(clip);

    if(push.have_pick == 1 && (params.pick_mode & 1) != 0)
    { // spot wb
      if(params.colour_mode == 0 || push.have_clut == 0)
        picked_rgb = params.cam_to_rec2020 * picked_rgb;
      else
        picked_rgb = process_clut(picked_rgb);
      picked_rgb /= picked_rgb.g;
      clip = cat16(clip, picked_rgb, params.mul.rgb);
    } // regular white balancing
    else clip = cat16(clip, vec3(1.0), params.mul.rgb);
    const float t = min(clip.r, min(clip.g, clip.b));
    rgb = min(rgb, vec3(t));
  } // end highlight clipping

  if(push.have_pick == 1 && (params.pick_mode & 2) != 0)
  { // deflicker based on input patch
    rgb *= 1.5/cam_lum;
  }
  rgb *= params.mul.w; // exposure correction

  if(params.N.x > 0)
  { // now rbf part:
    vec3 co = params.rbf_P * rgb;
    uint N = clamp(params.N.x, 0, 24);
    for(int i=0;i<N;i++) co += params.rbf_c[i].rgb * rbfk(rgb, params.rbf_p[i].rgb);
    rgb = co;
  }

  // dt ucs saturation last so we don't mess with the rbf which is potentially
  // used for cc24 calibration
  if(push.have_abney == 0 && params.saturation != 1.0)
  { // cut a few cycles if not needed
    rgb = max(rgb, vec3(0));
    vec3 xyY = rec2020_to_xyY(rgb);
    vec3 JCH = xyY_to_dt_UCS_JCH(xyY, 1.0);
    JCH.y = clamp(JCH.y * params.saturation, 0, 1.0);
    xyY = dt_UCS_JCH_to_xyY(JCH, 1.0);
    rgb = xyY_to_rec2020(xyY);
  }
  else if(push.have_abney == 1 && (params.saturation != 1.0 || params.gamut_mode > 0))
  { // saturation with hue constancy by dominant wavelength and gamut compression
    vec3 xyY = rec2020_to_xyY(rgb);

    // lookup wavelength and saturation from spectral upsampling table:
    tri2quad(xyY.xy);
    vec4 lut = texture(img_spectra, xyY.xy);
    vec2 sl = vec2(lut.w, -lut.y / (2.0 * lut.x));

    // translate wavelength to y coordinate in abney table:
    float norm = (sl.y - 400.0)/(700.0-400.0) - 0.5;
    sl.y = 0.5*(0.5 + 0.5 * norm / sqrt(norm*norm+0.25));
    if(lut.x > 0.0) sl.y += 0.5;

    // this is where we would be at without bounds
    float m = params.saturation * sl.x;
    // but we want to compress input in [sl.x.. infty) into
    // the interval [sl.x .. max_sat.x]
    const ivec2 size = textureSize(img_abney, 0).xy;
    if(params.gamut_mode > 0)
    {
      float bound = 1.0;
      if(params.gamut_mode == 1)
      { // spectral locus
        bound = texelFetch(img_abney, ivec2(size.x-1, sl.y*size.y), 0).g;
      }
      else if(params.gamut_mode == 2)
      { // rec2020
        vec2 max_sat = texelFetch(img_abney, ivec2(size.x-1, sl.y*size.y), 0).rg;
        bound = max_sat.x;
        sl.x *= max_sat.x / max_sat.y; // adjust lower bound to spectral locus scaled into triangle
        m = params.saturation * sl.x;
      }
      else if(params.gamut_mode == 3)
      { // rec709
        vec2 max_sat = texelFetch(img_abney, ivec2(size.x-2, sl.y*size.y), 0).rg;
        bound = max_sat.x;
        sl.x *= max_sat.x / max_sat.y;
        m = params.saturation * sl.x;
      }
      if(params.saturation > 1.0)
        sl.x = mix(sl.x, bound, (m - sl.x)/(m - sl.x + 1.0));
      else sl.x = m;
      if(sl.x > bound) sl.x = bound; // clip to gamut
    }
    sl.x = clamp(sl.x, 0.0, (size.x-3.0)/size.x); // make sure we don't hit the last column (gamut limits stored there)
    xyY.xy = texture(img_abney, sl).rg; // use lambda/sat lut to get new xy chroma values
    rgb = xyY_to_rec2020(xyY);
  }

  // emergency overflow clamping:
  rgb = clamp(rgb, vec3(-65535.0), vec3(65535.0));

  imageStore(img_out, ipos, vec4(rgb, 1));
}
