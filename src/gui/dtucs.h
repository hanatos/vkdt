/* The following is darktable Uniform Color Space 2022, MIT licence
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/ */

// maximum colourfulness for each hue angle.
// this is normalised in a way that it is almost independent of lightness.
// gamut bound is the spectral locus of the 1931 2 degree observer.
static const float dt_UCS_max_M[] = {
0.042296,0.042296,0.042296,0.042296,0.042296,0.042813,0.042813,0.042813,0.042813,0.042813,0.043329,0.043329,0.043329,0.043843,0.043843,0.044357,0.042813,0.039691,0.037051,0.035449,0.033832,0.032200,0.031102,0.029441,0.028883,0.027760,0.026629,0.026059,0.025487,0.024912,0.024335,0.023755,0.023172,0.022586,0.021997,0.021405,0.021405,0.020809,0.020210,0.020210,0.019608,0.019608,0.019002,0.019002,0.018391,0.018391,0.018391,0.017777,0.017777,0.017158,0.017158,0.017158,0.017158,0.016535,0.016535,0.016535,0.016535,0.015907,0.015907,0.015907,0.015907,0.015907,0.015907,0.015907,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.014636,0.014636,0.014636,0.014636,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015274,0.015907,0.015907,0.015907,0.015907,0.015907,0.015907,0.016535,0.016535,0.016535,0.016535,0.017158,0.017158,0.017158,0.017158,0.017777,0.017777,0.017777,0.018391,0.018391,0.019002,0.019002,0.019002,0.019608,0.019608,0.020210,0.020210,0.020809,0.020809,0.020809,0.021405,0.021405,0.021997,0.021997,0.022586,0.022586,0.023172,0.023755,0.023755,0.024335,0.024912,0.024912,0.025487,0.026059,0.026629,0.027196,0.027760,0.028323,0.028883,0.029441,0.029997,0.030550,0.031102,0.031652,0.032200,0.032746,0.033290,0.034373,0.034912,0.035449,0.035985,0.036519,0.036519,0.037051,0.037051,0.037582,0.037582,0.037582,0.037582,0.037051,0.037051,0.037051,0.036519,0.036519,0.036519,0.035985,0.035985,0.035449,0.035449,0.035449,0.034912,0.034912,0.034373,0.034373,0.033832,0.033832,0.033832,0.033290,0.033290,0.033290,0.032746,0.032746,0.032746,0.032746,0.032200,0.032200,0.032200,0.031652,0.031652,0.031652,0.031652,0.031652,0.031652,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031102,0.031652,0.031652,0.031652,0.031652,0.031652,0.032200,0.032200,0.032200,0.032200,0.032746,0.032746,0.032746,0.033290,0.033290,0.033290,0.033832,0.033832,0.034373,0.034373,0.034912,0.034912,0.035449,0.035449,0.035985,0.035985,0.036519,0.037051,0.037051,0.037582,0.038112,0.038640,0.039166,0.039691,0.040215,0.040737,0.041258,0.041778,0.042296,0.042813,0.043329,0.043843,0.044869,0.045380,0.046398,0.046906,0.047918,0.048925,0.049929,0.050928,0.051923,0.052915,0.053903,0.054887,0.056356,0.057818,0.059273,0.060720,0.062161,0.063595,0.065022,0.066915,0.068328,0.070203,0.072068,0.073923,0.076229,0.078063,0.079889,0.082160,0.084418,0.087111,0.089788,0.092891,0.095974,0.098600,0.099472,0.098163,0.096851,0.095535,0.094215,0.093333,0.091564,0.089343,0.087111,0.084418,0.081707,0.079433,0.077147,0.075308,0.072997,0.071137,0.069735,0.067858,0.066443,0.065022,0.063595,0.062640,0.061201,0.060239,0.059273,0.057818,0.056845,0.056356,0.055378,0.054395,0.053903,0.052915,0.052419,0.051426,0.050928,0.050429,0.049929,0.049427,0.048925,0.048422,0.047918,0.047412,0.046906,0.046398,0.046398,0.045890,0.045380,0.045380,0.044869,0.044869,0.044357,0.044357,0.043843,0.043843,0.043329,0.043329,0.043329,0.043329,0.042813,0.042813,0.042813,0.042813,0.042813,0.042296,0.042296,0.042296,0.042296,0.042296,
};

static inline float
Y_to_dt_UCS_L_star(const float Y)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = powf(Y, 0.631651345306265f);
  return 2.098883786377f * Y_hat / (Y_hat + 1.12426773749357f);
}
static inline float
dt_UCS_L_star_to_Y(const float L_star)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  return powf((1.12426773749357f * L_star / (2.098883786377f - L_star)), 1.5831518565279648f);
}
static inline struct nk_colorf
xy_to_dt_UCS_UV(struct nk_colorf xy)
{
  const float M1[] = { // column vectors:
      -0.783941002840055,  0.745273540913283, 0.318707282433486,
       0.277512987809202, -0.205375866083878, 2.16743692732158,
       0.153836578598858, -0.165478376301988, 0.291320554395942};
  float uvd[3] = {0.0f};
  for(int k=0;k<3;k++)
  {
    uvd[k] += M1[3*0+k] * xy.r;
    uvd[k] += M1[3*1+k] * xy.g;
    uvd[k] += M1[3*2+k] * 1.0f;
  }
  uvd[0] /= uvd[2];
  uvd[1] /= uvd[2];
  const float factors[]     = {1.39656225667, 1.4513954287 };
  const float half_values[] = {1.49217352929, 1.52488637914};
  float UV_star[] = {
    factors[0] * uvd[0] / (fabsf(uvd[0]) + half_values[0]),
    factors[1] * uvd[1] / (fabsf(uvd[1]) + half_values[1])};
  const float M2[] = {-1.124983854323892, 1.86323315098672, -0.980483721769325, 1.971853092390862};
  return (struct nk_colorf){
    M2[0] * UV_star[0] + M2[2] * UV_star[1],
    M2[1] * UV_star[0] + M2[3] * UV_star[1], 0, 0};
}
//    * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
//    * L_white the lightness of white as dt UCS L* lightness
//  range : xy in [0; 1], Y normalized for perfect diffuse white = 1
static inline struct nk_colorf
xyY_to_dt_UCS_JCH(const struct nk_colorf xyY, const float L_white)
{
  struct nk_colorf UV_star_prime = xy_to_dt_UCS_UV(xyY);
  const float L_star = Y_to_dt_UCS_L_star(xyY.b);
  float M2 = UV_star_prime.r*UV_star_prime.r + UV_star_prime.g*UV_star_prime.g; // square of colorfulness M
  float hue = atan2f(UV_star_prime.g, UV_star_prime.r);
  if(hue < 0.0f) hue += 2.0f*M_PI;
  float max_M = dt_UCS_max_M[CLAMP((int)(hue * 180.0f/M_PI + 0.5f), 0, 359)];
  M2 = CLAMP(M2, 0.0, max_M*max_M);
  return (struct nk_colorf){ // should be JCH[0] = powf(L_star / L_white), cz) but we treat only the case where cz = 1
      L_star / L_white,
      15.932993652962535f * powf(L_star, 0.6523997524738018f) * powf(M2, 0.6007557017508491f) / L_white,
      hue};
}
static inline float
dt_UCS_max_C(const struct nk_colorf JCH, const float L_white)
{
  float hue = JCH.b;
  if(hue < 0.0f) hue += 2.0f*M_PI;
  float max_M = dt_UCS_max_M[CLAMP((int)(hue * 180.0f/M_PI + 0.5f), 0, 359)];
  float M2 = max_M * max_M;
  const float L_star = JCH.r * L_white;
  float C = 15.932993652962535f * powf(L_star, 0.6523997524738018f) * powf(M2, 0.6007557017508491f) / L_white;
  // now quantise it a bit so it doesn't drift when calling this in iteration
  // XXX does not help!
  const int N = 1000;
  return (int)(C*N + 0.5f)/(float)N;
}
static inline struct nk_colorf
dt_UCS_JCH_to_xyY(const struct nk_colorf JCH, const float L_white)
{
  const float L_star = JCH.r * L_white; // should be L_star = powf(JCH[0], 1.f / cz) * L_white but we treat only cz = 1
  float M = powf(JCH.g * L_white / (15.932993652962535f * powf(L_star, 0.6523997524738018f)), 0.8322850678616855f);

  float hue = JCH.b;
  if(hue < 0.0f) hue += 2.0f*M_PI;
  float max_M = dt_UCS_max_M[CLAMP((int)(hue * 180.0f/M_PI + 0.5f), 0, 359)];
  M = CLAMP(M, 0.0, max_M);

  float UV_starp[] = { M * cosf(JCH.b), M * sin(JCH.b) }; // uv*'
  const float M1[] = {-5.037522385190711, 4.760029407436461, -2.504856328185843, 2.874012963239247}; // col major
  float UV_star[] = {
    M1[0] * UV_starp[0] + M1[2] * UV_starp[1],
    M1[1] * UV_starp[0] + M1[3] * UV_starp[1]};
  const float factors[]     = {1.39656225667, 1.4513954287};
  const float half_values[] = {1.49217352929, 1.52488637914};
  float UV[] = {
    -half_values[0] * UV_star[0] / (fabsf(UV_star[0]) - factors[0]),
    -half_values[1] * UV_star[1] / (fabsf(UV_star[1]) - factors[1])};
  const float M2[] = { // given as column vectors
       0.167171472114775,   -0.150959086409163,    0.940254742367256,
       0.141299802443708,   -0.155185060382272,    1.000000000000000,
      -0.00801531300850582, -0.00843312433578007, -0.0256325967652889};
  float xyD[3] = {0.0f};
  for(int k=0;k<3;k++)
  {
    xyD[k] += M2[3*0+k] * UV[0];
    xyD[k] += M2[3*1+k] * UV[1];
    xyD[k] += M2[3*2+k] * 1.0f;
  }
  return (struct nk_colorf){xyD[0]/xyD[2], xyD[1]/xyD[2], dt_UCS_L_star_to_Y(L_star)};
}
static inline struct nk_colorf
dt_UCS_JCH_to_HSB(const struct nk_colorf JCH)
{
  struct nk_colorf HSB;
  HSB.b = JCH.r * (powf(JCH.g, 1.33654221029386f) + 1.f);
  return (struct nk_colorf){JCH.b, (HSB.b > 0.f) ? JCH.g / HSB.b : 0.f, HSB.b};
}
static inline struct nk_colorf
dt_UCS_HSB_to_JCH(const struct nk_colorf HSB)
{
  return (struct nk_colorf){HSB.b / (powf(HSB.g*HSB.b, 1.33654221029386f) + 1.f), HSB.g * HSB.b, HSB.r};
}
static inline struct nk_colorf
dt_UCS_JCH_to_HCB(const struct nk_colorf JCH)
{
  return (struct nk_colorf){JCH.b, JCH.g, JCH.r * (powf(JCH.g, 1.33654221029386f) + 1.f)};
}
static inline struct nk_colorf
dt_UCS_HCB_to_JCH(const struct nk_colorf HCB)
{
  return (struct nk_colorf){HCB.b / (powf(HCB.g, 1.33654221029386f) + 1.f), HCB.g, HCB.r};
}
static inline void
rec2020_to_dtucs(const float rgb[], float dtucs[])
{ // these matrices are the same as in glsl, i.e. transposed:
  const float rec2020_to_xyz[] = {
    6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
    1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
    1.68880975e-01, 5.93017165e-02, 1.06098506e+00};
  float xyz[3] = {0.0f};
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      xyz[k] += rec2020_to_xyz[3*i+k] * rgb[i];
  float s = 1.0f/(xyz[0]+xyz[1]+xyz[2]);
  struct nk_colorf xyY = { s*xyz[0], s*xyz[1], xyz[1] };
  struct nk_colorf jch = xyY_to_dt_UCS_JCH(xyY, 1.0f);
  float max_C = dt_UCS_max_C(jch, 1.0f);
  // struct nk_colorf hsb = dt_UCS_JCH_to_HSB(jch);
  // dtucs[0] = hsb.r; dtucs[1] = hsb.g; dtucs[2] = hsb.b;
  // struct nk_colorf hcb = dt_UCS_JCH_to_HCB(jch);
  // dtucs[0] = hcb.r; dtucs[1] = CLAMP(hcb.g/max_C, 0, 1); dtucs[2] = hcb.b;
  dtucs[0] = jch.b; dtucs[1] = CLAMP(jch.g/max_C, 0, 1); dtucs[2] = jch.r;
}
static inline void
dtucs_to_rec2020(const float dtucs[], float rgb[])
{
  // struct nk_colorf hsb = {dtucs[0],dtucs[1],dtucs[2]};
  // struct nk_colorf jch = dt_UCS_HSB_to_JCH(hsb);
  // struct nk_colorf hcb = {dtucs[0],dtucs[1],dtucs[2]};
  // struct nk_colorf jch = dt_UCS_HCB_to_JCH(hcb);
  struct nk_colorf jch = {dtucs[2],dtucs[1],dtucs[0]};
  float max_C = dt_UCS_max_C(jch, 1.0f);
  jch.g = CLAMP(jch.g, 0, 1)*max_C;
  struct nk_colorf xyY = dt_UCS_JCH_to_xyY(jch, 1.0f);
  float s = xyY.b / xyY.g;
  float xyz[] = {s*xyY.r, s*xyY.g, s*(1.0-xyY.r-xyY.g)};
  const float xyz_to_rec2020[] = {
    1.71665119, -0.66668435,  0.01763986,
   -0.35567078,  1.61648124, -0.04277061,
   -0.25336628,  0.01576855,  0.94210312};
  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      rgb[k] += xyz_to_rec2020[3*i+k] * xyz[i];
}
