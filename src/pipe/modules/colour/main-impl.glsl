#include "shared.glsl"
#include "colourspaces.glsl"
#include "shared/dtucs.glsl"
#include "shared/oetf.glsl"

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

layout(set = 1, binding = 0) uniform sampler2D img_in;
layout(set = 1, binding = 1) uniform writeonly image2D img_out;
layout(set = 1, binding = 2) uniform sampler2D img_clut;
layout(set = 1, binding = 3
#ifdef HAVE_NO_ATOMICS
) uniform usampler2D img_pick;
#else
) uniform sampler2D img_pick;
#endif
layout(set = 1, binding = 4) uniform sampler2D img_abney;
layout(set = 1, binding = 5) uniform sampler2D img_spectra;

vec3 // return adapted rec2020
cat16(vec3 rec2020_d65, vec3 rec2020_src, vec3 rec2020_dst)
{
  // these are the CAT16 M^{-1} and M matrices.
  // we use the standalone adaptation as proposed in
  // Smet and Ma, "Some concerns regarding the CAT16 chromatic adaptation transform",
  // Color Res Appl. 2020;45:172–177.
  // these are XYZ to cone-like
  const mat3 M16i = matrix_cat16_Mi;
  const mat3 M16  = matrix_cat16_M;
  const mat3 rec2020_to_xyz = matrix_rec2020_to_xyz;
  const mat3 xyz_to_rec2020 = matrix_xyz_to_rec2020;

  const vec3 cl_src = M16 * rec2020_to_xyz * rec2020_src;
  const vec3 cl_dst = M16 * rec2020_to_xyz * rec2020_dst;
  vec3 cl = M16 * rec2020_to_xyz * rec2020_d65;
  cl *= cl_dst / cl_src;
  return xyz_to_rec2020 * M16i * cl;
}

float
rbfk(vec3 ci, vec3 p)
{ // thinplate spline kernel
  float r2 = dot(ci-p, ci-p);
  return sqrt(r2);
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
  else if(params.trc == 6) // gamma 2.2
    rgb = pow(max(rgb, vec3(0.0)), vec3(2.2)); // happens to be adobe rgb
  else if(params.trc == 7) // davinci intermediate
    rgb = oetf_davinci_intermediate(rgb);
  else if(params.trc == 8) // filmlight t-log
    rgb = oetf_filmlight_tlog(rgb);
  else if(params.trc == 9) // aces cct
    rgb = oetf_acescct(rgb);
  else if(params.trc == 10) // arri logC3
    rgb = oetf_arri_logc3(rgb);
  else if(params.trc == 11) // arri logC4
    rgb = oetf_arri_logc4(rgb);
  else if(params.trc == 12) // red log3G10
    rgb = oetf_red_log3g10(rgb);
  else if(params.trc == 13) // panasonic v-log
    rgb = oetf_panasonic_vlog(rgb);
  else if(params.trc == 14) // sony s-log3
    rgb = oetf_sony_slog3(rgb);
  else if(params.trc == 15) // fuji f-log2
    rgb = oetf_fujifilm_flog2(rgb);


  if(params.primaries == 0)
  { // use uploaded custom matrix
    rgb = params.cam_to_rec2020 * rgb;
  }
#define TO2020(I, P) \
  else if(params.primaries == I)\
  {\
    const mat3 M = matrix_ ## P ## _to_rec2020;\
    rgb = M * rgb; \
  }
  TO2020(1, rec709)
  // 2 is rec2020 already
  TO2020(3, adobergb)
  TO2020(4, p3d65)
  TO2020(5, xyz)
  TO2020(6, ap0)
  TO2020(7, ap1)
  TO2020(10, redwg)
#undef TO2020
#define TO2020(I, P) \
  else if(params.primaries == I)\
  {\
    const mat3 M0 = matrix_ ## P ## _to_xyz;\
    const mat3 M1 = matrix_xyz_to_rec2020;\
    rgb = M1 * M0 * rgb; \
  }
  TO2020( 8, arriwg3)
  TO2020( 9, arriwg4)
  TO2020(11, sonysgamut3)
  TO2020(12, sonysgamut3cine)
  TO2020(13, vgamut)
  TO2020(14, egamut)
  TO2020(15, egamut2)
  TO2020(16, davinciwg)
#undef TO2020
  return rgb;
}

void
main()
{
  ivec2 ipos = ivec2(gl_GlobalInvocationID);
  if(any(greaterThanEqual(ipos, imageSize(img_out)))) return;

  vec3 rgb = texture(img_in, (ipos+0.5)/vec2(imageSize(img_out))).rgb; // read camera rgb
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
