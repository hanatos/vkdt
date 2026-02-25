/* OpenDRT v1.1.0 -------------------------------------------------

   Written by Jed Smith
https://github.com/jedypod/open-display-transform
License: GPLv3
-------------------------------------------------*/

#include "matrices.h"

/* Math helper functions ----------------------------*/

#define SQRT3 1.73205080756887729353f

// Safe power function raising float a to power float b
float spowf(float a, float b) {
  return mix(pow(a, b), a, a <= 0.0);
}

// Safe power function raising vec3 a to power float b
vec3 spowf3(vec3 a, float b) {
  return mix(pow(a, vec3(b)), a, lessThanEqual(a, vec3(0.0)));
}
// Return the hypot or vector length of vec2 v
float hypotf2(vec2 v) { return sqrt(max(0.0, v.x*v.x + v.y*v.y)); }

// Return the hypot or vector length of vec3 v
float hypotf3(vec3 v) { return sqrt(max(0.0, v.x*v.x + v.y*v.y + v.z*v.z)); }

// Return the min of vec3 a
float fmaxf3(vec3 a) { return max(a.x, max(a.y, a.z)); }

// Return the max of vec3 a
float fminf3(vec3 a) { return min(a.x, min(a.y, a.z)); }

float exp10(float v) { return pow(10.0, v); }

/* OETF Linearization Transfer Functions ---------------------------------------- */

float oetf_davinci_intermediate(float x) {
    return x <= 0.02740668f ? x/10.44426855f : exp2(x/0.07329248f - 7.0f) - 0.0075f;
}
float oetf_filmlight_tlog(float x) {
  return x < 0.075f ? (x-0.075f)/16.184376489665897f : exp((x - 0.5520126568606655f)/0.09232902596577353f) - 0.0057048244042473785f;
}
float oetf_acescct(float x) {
  return x <= 0.155251141552511f ? (x - 0.0729055341958355f)/10.5402377416545f : exp2(x*17.52f - 9.72f);
}
float oetf_arri_logc3(float x) {
  return x < 5.367655f*0.010591f + 0.092809f ? (x - 0.092809f)/5.367655f : (exp10((x - 0.385537f)/0.247190f) - 0.052272f)/5.555556f;
}
float oetf_arri_logc4(float x) {
  return x < -0.7774983977293537f ? x*0.3033266726886969f - 0.7774983977293537f : (exp2(14.0f*(x - 0.09286412512218964f)/0.9071358748778103f + 6.0f) - 64.0f)/2231.8263090676883f;
}
float oetf_red_log3g10(float x) {
  return x < 0.0f ? (x/15.1927f) - 0.01f : (exp10(x/0.224282f) - 1.0f)/155.975327f - 0.01f;
}
float oetf_panasonic_vlog(float x) {
  return x < 0.181f ? (x - 0.125f)/5.6f : exp10((x - 0.598206f)/0.241514f) - 0.00873f;
}
float oetf_sony_slog3(float x) {
  return x < 171.2102946929f/1023.0f ? (x*1023.0f - 95.0f)*0.01125f/(171.2102946929f - 95.0f) : (exp10(((x*1023.0f - 420.0f)/261.5f))*(0.18f + 0.01f) - 0.01f);
}
float oetf_fujifilm_flog2(float x) {
  return x < 0.100686685370811f ? (x - 0.092864f)/8.799461f : (exp10(((x - 0.384316f)/0.245281f))/5.555556f - 0.064829f/5.555556f);
}


vec3 linearize(vec3 rgb, int tf) {
  if (tf==0) { // Linear
    return rgb;
  } else if (tf==1) { // Davinci Intermediate
    rgb.x = oetf_davinci_intermediate(rgb.x);
    rgb.y = oetf_davinci_intermediate(rgb.y);
    rgb.z = oetf_davinci_intermediate(rgb.z);
  } else if (tf==2) { // Filmlight T-Log
    rgb.x = oetf_filmlight_tlog(rgb.x);
    rgb.y = oetf_filmlight_tlog(rgb.y);
    rgb.z = oetf_filmlight_tlog(rgb.z);
  } else if (tf==3) { // ACEScct
    rgb.x = oetf_acescct(rgb.x);
    rgb.y = oetf_acescct(rgb.y);
    rgb.z = oetf_acescct(rgb.z);
  } else if (tf==4) { // Arri LogC3
    rgb.x = oetf_arri_logc3(rgb.x);
    rgb.y = oetf_arri_logc3(rgb.y);
    rgb.z = oetf_arri_logc3(rgb.z);
  } else if (tf==5) { // Arri LogC4
    rgb.x = oetf_arri_logc4(rgb.x);
    rgb.y = oetf_arri_logc4(rgb.y);
    rgb.z = oetf_arri_logc4(rgb.z);
  } else if (tf==6) { // RedLog3G10
    rgb.x = oetf_red_log3g10(rgb.x);
    rgb.y = oetf_red_log3g10(rgb.y);
    rgb.z = oetf_red_log3g10(rgb.z);
  } else if (tf==7) { // Panasonic V-Log
    rgb.x = oetf_panasonic_vlog(rgb.x);
    rgb.y = oetf_panasonic_vlog(rgb.y);
    rgb.z = oetf_panasonic_vlog(rgb.z);
  } else if (tf==8) { // Sony S-Log3
    rgb.x = oetf_sony_slog3(rgb.x);
    rgb.y = oetf_sony_slog3(rgb.y);
    rgb.z = oetf_sony_slog3(rgb.z);
  } else if (tf==9) { // Fuji F-Log2
    rgb.x = oetf_fujifilm_flog2(rgb.x);
    rgb.y = oetf_fujifilm_flog2(rgb.y);
    rgb.z = oetf_fujifilm_flog2(rgb.z);
  }  return rgb;
}



/* EOTF Transfer Functions ---------------------------------------- */

vec3 eotf_hlg(vec3 rgb, int inverse) {
  /* Apply the HLG Forward or Inverse EOTF for 1000 nits.
      ITU-R Rec BT.2100-2 https://www.itu.int/rec/R-REC-BT.2100
      ITU-R Rep BT.2390-8: https://www.itu.int/pub/R-REP-BT.2390
  */

  if (inverse == 1) {
    float Yd = 0.2627f*rgb.x + 0.6780f*rgb.y + 0.0593f*rgb.z;
    rgb = rgb*spowf(Yd, (1.0f - 1.2f)/1.2f);
    rgb.x = rgb.x <= 1.0f/12.0f ? sqrt(3.0f*rgb.x) : 0.17883277f*log(12.0f*rgb.x - 0.28466892f) + 0.55991073f;
    rgb.y = rgb.y <= 1.0f/12.0f ? sqrt(3.0f*rgb.y) : 0.17883277f*log(12.0f*rgb.y - 0.28466892f) + 0.55991073f;
    rgb.z = rgb.z <= 1.0f/12.0f ? sqrt(3.0f*rgb.z) : 0.17883277f*log(12.0f*rgb.z - 0.28466892f) + 0.55991073f;
  } else {
    rgb.x = rgb.x <= 0.5f ? rgb.x*rgb.x/3.0f : (exp((rgb.x - 0.55991073f)/0.17883277f) + 0.28466892f)/12.0f;
    rgb.y = rgb.y <= 0.5f ? rgb.y*rgb.y/3.0f : (exp((rgb.y - 0.55991073f)/0.17883277f) + 0.28466892f)/12.0f;
    rgb.z = rgb.z <= 0.5f ? rgb.z*rgb.z/3.0f : (exp((rgb.z - 0.55991073f)/0.17883277f) + 0.28466892f)/12.0f;
    float Ys = 0.2627f*rgb.x + 0.6780f*rgb.y + 0.0593f*rgb.z;
    rgb = rgb*spowf(Ys, 1.2f - 1.0f);
  }
  return rgb;
}


vec3 eotf_pq(vec3 rgb, int inverse) {
  /* Apply the ST-2084 PQ Forward or Inverse EOTF
      ITU-R Rec BT.2100-2 https://www.itu.int/rec/R-REC-BT.2100
      ITU-R Rep BT.2390-9 https://www.itu.int/pub/R-REP-BT.2390
      Note: in the spec there is a normalization for peak display luminance.
      For this function we assume the input is already normalized such that 1.0 = 10,000 nits
  */

  const float m1 = 2610.0f/16384.0f;
  const float m2 = 2523.0f/32.0f;
  const float c1 = 107.0f/128.0f;
  const float c2 = 2413.0f/128.0f;
  const float c3 = 2392.0f/128.0f;

  if (inverse == 1) {
    rgb = spowf3(rgb, m1);
    rgb = spowf3((c1 + c2*rgb)/(1.0f + c3*rgb), m2);
  } else {
    rgb = spowf3(rgb, 1.0f/m2);
    rgb = spowf3((rgb - c1)/(c2 - c3*rgb), 1.0f/m1);
  }
  return rgb;
}


/* Functions for OpenDRT ---------------------------------------- */

float compress_hyperbolic_power(float x, float s, float p) {
  // Simple hyperbolic compression function https://www.desmos.com/calculator/ofwtcmzc3w
  return spowf(x/(x + s), p);
}

float compress_toe_quadratic(float x, float toe, int inv) {
  // Quadratic toe compress function https://www.desmos.com/calculator/skk8ahmnws
  if (toe == 0.0f) return x;
  if (inv == 0) {
    return spowf(x, 2.0f)/(x + toe);
  } else {
    return (x + sqrt(x*(4.0f*toe + x)))/2.0f;
  }
}

float compress_toe_cubic(float x, float m, float w, int inv) {
  // https://www.desmos.com/calculator/ubgteikoke
  if (m==1.0f) return x;
  float x2 = x*x;
  if (inv == 0) {
    return x*(x2 + m*w)/(x2 + w);
  } else {
    float p0 = x2 - 3.0f*m*w;
    float p1 = 2.0f*x2 + 27.0f*w - 9.0f*m*w;
    float p2 = pow(sqrt(x2*p1*p1 - 4*p0*p0*p0)/2.0f + x*p1/2.0f, 1.0f/3.0f);
    return p0/(3.0f*p2) + p2/3.0f + x/3.0f;
  }
}

float complement_power(float x, float p) {
  return 1.0f - spowf(1.0f - x, 1.0f/p);
}

float sigmoid_cubic(float x, float s) {
  // Simple cubic sigmoid: https://www.desmos.com/calculator/hzgib42en6
  if (x < 0.0f || x > 1.0f) return 1.0f;
  return 1.0f + s*(1.0f - 3.0f*x*x + 2.0f*x*x*x);
}

float contrast_high(float x, float p, float pv, float pv_lx, int inv) {
  // High exposure adjustment with linear extension
  // https://www.desmos.com/calculator/etjgwyrgad
  const float x0 = 0.18f*pow(2.0f, pv);
  if (x < x0 || p == 1.0f) return x;

  const float o = x0 - x0/p;
  const float s0 = pow(x0, 1.0f - p)/p;
  const float x1 = x0*pow(2.0f, pv_lx);
  const float k1 = p*s0*pow(x1, p)/x1;
  const float y1 = s0*pow(x1, p) + o;
  if (inv==1)
    return x > y1 ? (x - y1)/k1 + x1 : pow((x - o)/s0, 1.0f/p);
  else
    return x > x1 ? k1*(x - x1) + y1 : s0*pow(x, p) + o;
}

float softplus_constraint(float x, float s, float x0, float y0) {
  // Softplus with (x0, y0) intersection constraint
  // https://www.desmos.com/calculator/doipi4u0ce
  if (x > 10.0f*s + y0 || s < 1e-3f) return x;
  float m = 1.0f;
  if (abs(y0) > 1e-6f) m = exp(y0/s);
  m -= exp(x0/s);
  return s*log(max(0.0f, m + exp(x/s)));
}

float softplus(float x, float s) {
  // Softplus unconstrained
  // https://www.desmos.com/calculator/mr9rmujsmn
  if (x > 10.0f*s || s < 1e-4f) return x;
  return s*log(max(0.0f, 1.0f + exp(x/s)));
}

float gauss_window(float x, float w) {
  // Simple gaussian window https://www.desmos.com/calculator/vhr9hstlyk
  return exp(-x*x/w);
}

vec2 opponent(vec3 rgb) {
  // Simple Cyan-Yellow / Green-Magenta opponent space for calculating smooth achromatic distance and hue angles
  return vec2(rgb.x - rgb.z, rgb.y - (rgb.x + rgb.z)/2.0f);
}

float hue_offset(float h, float o) {
  // Offset hue maintaining 0-2*pi range with modulo
  return mod(h - o + M_PI, 2.0*M_PI) - M_PI;
}



vec3 display_gamut_whitepoint(vec3 rgb, float tsn, float cwp_lm, int display_gamut, int cwp) {
  // Do final display gamut and creative whitepoint conversion. 
  // Must be done twice for the tonescale overlay, thus a separate function.
  
  // First, convert from P3D65 to XYZ D65
  rgb = matrix_p3d65_to_xyz * rgb;

  // Store "neutral" axis for mixing with Creative White Range control
  vec3 cwp_neutral = rgb;
  
  float cwp_f = pow(tsn, 2.0f*cwp_lm);
  
  if (display_gamut < 3) { // D65 aligned P3 or Rec.709 display gamuts
    if (cwp==0) rgb = matrix_cat_d65_to_d93 * rgb; // D93
    else if (cwp==1) rgb = matrix_cat_d65_to_d75 * rgb; // D75
    // else if (cwp==2) rgb = matrix_cat_d65_to_d60, rgb; // D65
    else if (cwp==3) rgb = matrix_cat_d65_to_d60 * rgb; // D60
    else if (cwp==4) rgb = matrix_cat_d65_to_d55 * rgb; // D55
    else if (cwp==5) rgb = matrix_cat_d65_to_d50 * rgb; // D50
  } 
  else if (display_gamut == 3) { // P3-D60
    if (cwp==0) rgb = matrix_cat_d60_to_d93 * rgb; // D93
    else if (cwp==1) rgb = matrix_cat_d60_to_d75 * rgb; // D75
    else if (cwp==2) rgb = matrix_cat_d60_to_d65 * rgb; // D65
    // D60
    else if (cwp==4) rgb = matrix_cat_d60_to_d55 * rgb; // D55
    else if (cwp==5) rgb = matrix_cat_d60_to_d50 * rgb; // D50
  } 
  else { // DCI P3 or DCI X'Y'Z'
    // Keep "Neutral" axis as D65, don't want green midtones in P3-DCI container.
    cwp_neutral = matrix_cat_dci_to_d65 * rgb;
    if (cwp==0) rgb = matrix_cat_dci_to_d93 * rgb; // D93
    else if (cwp==1) rgb = matrix_cat_dci_to_d75 * rgb; // D75
    else if (cwp==2) rgb = cwp_neutral;
    else if (cwp==3) rgb = matrix_cat_dci_to_d60 * rgb; // D60
    else if (cwp==4) rgb = matrix_cat_dci_to_d55 * rgb; // D55
    else if (cwp==5) rgb = matrix_cat_dci_to_d50 * rgb; // D50
  }
  
  // Mix between Creative Whitepoint and "neutral" axis with Creative White Range control.
  rgb = rgb*cwp_f + cwp_neutral*(1.0f - cwp_f);


  // RGB is now aligned to the selected creative white
  // and we can convert back to the final target display gamut
  if (display_gamut == 0) { // Rec.709
    rgb = matrix_xyz_to_rec709 * rgb;
  } 
  else if (display_gamut == 5) { // DCDM X'Y'Z'
    // Convert whitepoint from D65 to DCI
    rgb = matrix_cat_d65_to_dci * rgb;
  }
  else { // For all others, convert to P3D65
    rgb = matrix_xyz_to_p3d65 * rgb;
  }

  // Post creative whitepoint normalization so that peak luminance does not exceed display maximum.
  // We could calculate this by storing a 1,1,1 value in p3d65 and then normalize by the result through the cat and xyz to rgb matrix. 
  // Instead we use pre-calculated constants to avoid the extra calculations.
    
  /* Pre-calculated normalization factors are inline below
  */

  float cwp_norm = 1.0f;
  /* Display Gamut - Rec.709
    rec709 d93: 0.744192699063f
    rec709 d75: 0.873470832146f
    rec709 d60: 0.955936992163f
    rec709 d55: 0.905671332781f
    rec709 d50: 0.850004385027f
  */
  if (display_gamut == 0) { // Rec.709
    if (cwp == 0) cwp_norm = 0.744192699063f; // D93
    else if (cwp == 1) cwp_norm = 0.873470832146f; // D75
    // else if (cwp == 2) cwp_norm = 1.0f; // D65
    else if (cwp == 3) cwp_norm = 0.955936992163f; // D60
    else if (cwp == 4) cwp_norm = 0.905671332781f; // D55
    else if (cwp == 5) cwp_norm = 0.850004385027f; // D50
  }
  /* Display Gamut - P3D65
    p3d65 d93: 0.762687057298f
    p3d65 d75: 0.884054083328f
    p3d65 d60: 0.964320186739f
    p3d65 d55: 0.923076518860f
    p3d65 d50: 0.876572837784f
  */
  else if (display_gamut == 1 || display_gamut == 2) { // P3D65 or P3 Limited Rec.2020
    if (cwp == 0) cwp_norm = 0.762687057298f; // D93
    else if (cwp == 1) cwp_norm = 0.884054083328f; // D75
    // else if (cwp == 2) cwp_norm = 1.0f; // D65
    else if (cwp == 3) cwp_norm = 0.964320186739f; // D60
    else if (cwp == 4) cwp_norm = 0.923076518860f; // D55
    else if (cwp == 5) cwp_norm = 0.876572837784f; // D50
  }
  /* Display Gamut - P3D60
    p3d60 d93: 0.704956321013f
    p3d60 d75: 0.816715709816f
    p3d60 d65: 0.923382193663f
    p3d60 d55: 0.956138500287f
    p3d60 d50: 0.906801453023f
  */
  else if (display_gamut == 3) { // P3D60
    if (cwp == 0) cwp_norm = 0.704956321013f; // D93
    else if (cwp == 1) cwp_norm = 0.816715709816f; // D75
    else if (cwp == 2) cwp_norm = 0.923382193663f; // D65
    // else if (cwp == 3) cwp_norm = 1.0f; // D60
    else if (cwp == 4) cwp_norm = 0.956138500287f; // D55
    else if (cwp == 5) cwp_norm = 0.906801453023f; // D50
  }
  /* Display Gamut - P3-DCI
    p3dci d93: 0.665336141225f
    p3dci d75: 0.770397131382f
    p3dci d65: 0.870572343302f
    p3dci d60: 0.891354547503f
    p3dci d55: 0.855327825187f
    p3dci d50: 0.814566436117f
*/
  else if (display_gamut == 4) { // P3DCI
    if (cwp == 0) cwp_norm = 0.665336141225f; // D93
    else if (cwp == 1) cwp_norm = 0.770397131382f; // D75
    else if (cwp == 2) cwp_norm = 0.870572343302f; // D65
    else if (cwp == 3) cwp_norm = 0.891354547503f; // D60
    else if (cwp == 4) cwp_norm = 0.855327825187f; // D55
    else if (cwp == 5) cwp_norm = 0.814566436117f; // D50
  }
  /* Display Gamut - DCDM XYZ
    p3dci d93: 0.707142784007f
    p3dci d75: 0.815561082617f
    */
  else if (display_gamut == 5) { // DCDM X'Y'Z'
    if (cwp == 0) cwp_norm =0.707142784007f; // D93
    else if (cwp == 1) cwp_norm = 0.815561082617f; // D75
    else if (cwp >= 2) cwp_norm = 0.916555279740f; // 48/52.37 for D65 and warmer (see DCI spec)
  }
  
  // only normalize values affected by range control
  rgb *= cwp_norm*cwp_f + 1.0f - cwp_f;
  
  return rgb;
}

