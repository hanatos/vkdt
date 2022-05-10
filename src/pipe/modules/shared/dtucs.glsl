/** The following is darktable Uniform Color Space 2022
 * © Aurélien Pierre
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/
 *
 * Use this space for color-grading in a perceptual framework.
 * The CAM terms have been removed for performance.
 **/

// to get to hue chroma brightness, we'd call:
// float4 xyY = dt_XYZ_to_xyY(XYZ_D65);
// float4 JCH = xyY_to_dt_UCS_JCH(xyY, L_white);
// float4 HCB = dt_UCS_JCH_to_HCB(JCH);

float Y_to_dt_UCS_L_star(const float Y)
{
  // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = pow(Y, 0.631651345306265f);
  return 2.098883786377f * Y_hat / (Y_hat + 1.12426773749357f);
}

float dt_UCS_L_star_to_Y(const float L_star)
{
  // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  return pow((1.12426773749357f * L_star / (2.098883786377f - L_star)), 1.5831518565279648f);
}

vec2 xyY_to_dt_UCS_UV(vec2 xy)
{
  const vec3 x_factors = vec3(-0.783941002840055f,  0.745273540913283f, 0.318707282433486f);
  const vec3 y_factors = vec3( 0.277512987809202f, -0.205375866083878f, 2.16743692732158f);
  const vec3 offsets   = vec3( 0.153836578598858f, -0.165478376301988f, 0.291320554395942f);

  vec3 uvd = x_factors * xy.x + y_factors * xy.y + offsets;
  uvd.xy /= uvd.z;

  const vec2 factors     = vec2(1.39656225667f, 1.4513954287f );
  const vec2 half_values = vec2(1.49217352929f, 1.52488637914f);
  vec2 UV_star = factors * uvd.xy / (abs(uvd.xy) + half_values);

  // The following is equivalent to a 2D matrix product
  return vec2(
      -1.124983854323892f * UV_star.x - 0.980483721769325f * UV_star.y,
       1.86323315098672f  * UV_star.x + 1.971853092390862f * UV_star.y);
}


vec3 xyY_to_dt_UCS_JCH(const vec3 xyY, const float L_white)
{
  /*
    input :
      * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
      * L_white the lightness of white as dt UCS L* lightness
      * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
              (background = middle grey, white = perfect diffuse white)
    range : xy in [0; 1], Y normalized for perfect diffuse white = 1
  */

  vec2 UV_star_prime = xyY_to_dt_UCS_UV(xyY.xy);
  const float L_star = Y_to_dt_UCS_L_star(xyY.z);
  const float M2 = dot(UV_star_prime, UV_star_prime); // square of colorfulness M

  // should be JCH[0] = powf(L_star / L_white), cz) but we treat only the case where cz = 1
  return vec3(
      L_star / L_white,
      15.932993652962535f * pow(L_star, 0.6523997524738018f) * pow(M2, 0.6007557017508491f) / L_white,
      atan(UV_star_prime.y, UV_star_prime.x));
}


vec3 dt_UCS_JCH_to_xyY(const vec3 JCH, const float L_white)
{
  /*
    input :
      * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
      * L_white the lightness of white as dt UCS L* lightness
      * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
              (background = middle grey, white = perfect diffuse white)
    range : xy in [0; 1], Y normalized for perfect diffuse white = 1
  */

  // should be L_star = powf(JCH[0], 1.f / cz) * L_white but we treat only the case where cz = 1
  const float L_star = JCH.x * L_white;
  const float M = pow(JCH.y * L_white / (15.932993652962535f * pow(L_star, 0.6523997524738018f)), 0.8322850678616855f);

  const float U_star_prime = M * cos(JCH.z);
  const float V_star_prime = M * sin(JCH.z);

  // The following is equivalent to a 2D matrix product
  const vec2 UV_star = vec2( -5.037522385190711f * U_star_prime - 2.504856328185843f * V_star_prime,
                              4.760029407436461f * U_star_prime + 2.874012963239247f * V_star_prime);

  const vec2 factors     = vec2(1.39656225667f, 1.4513954287f);
  const vec2 half_values = vec2(1.49217352929f, 1.52488637914f);
  vec2 UV = -half_values * UV_star / (abs(UV_star) - factors);

  const vec3 U_factors = vec3( 0.167171472114775f,   -0.150959086409163f,    0.940254742367256f);
  const vec3 V_factors = vec3( 0.141299802443708f,   -0.155185060382272f,    1.000000000000000f);
  const vec3 offsets   = vec3(-0.00801531300850582f, -0.00843312433578007f, -0.0256325967652889f);

  vec3 xyD = U_factors * UV.x + V_factors * UV.y + offsets;
  return vec3(xyD.xy / xyD.z, dt_UCS_L_star_to_Y(L_star));
}

vec3 dt_UCS_JCH_to_HSB(const vec3 JCH)
{
  vec3 HSB;
  HSB.z = JCH.x * (pow(JCH.y, 1.33654221029386f) + 1.f);
  return vec3(JCH.z, (HSB.z > 0.f) ? JCH.y / HSB.z : 0.f, HSB.z);
}

vec3 dt_UCS_HSB_to_JCH(const vec3 HSB)
{
  return vec3(HSB.z / (pow(HSB.x*HSB.z, 1.33654221029386f) + 1.f), HSB.y * HSB.z, HSB.x);
}

vec3 dt_UCS_JCH_to_HCB(const vec3 JCH)
{
  return vec3(JCH.zy, JCH.x * (pow(JCH.y, 1.33654221029386f) + 1.f));
}

vec3 dt_UCS_HCB_to_JCH(const vec3 HCB)
{
  return vec3(HCB.z / (pow(HCB.y, 1.33654221029386f) + 1.f), HCB.y, HCB.x);
}













#if 0 // whatever tf this is it doesn't seem to be needed
static inline void dt_UCS_HSB_to_HPW(const dt_aligned_pixel_t HSB, dt_aligned_pixel_t HPW)
{
  HPW[2] = sqrtf(HSB[1] * HSB[1] + HSB[2] * HSB[2]);
  HPW[1] = (HPW[2] > 0.f) ? HSB[1] / HPW[2] : 0.f;
  HPW[0] = HSB[0];
}


static inline void dt_UCS_HPW_to_HSB(const dt_aligned_pixel_t HPW, dt_aligned_pixel_t HSB)
{
  HSB[0] = HPW[0];
  HSB[1] = HPW[1] * HPW[2];
  HSB[2] = fmaxf(sqrtf(HPW[2] * HPW[2] - HSB[1] * HSB[1]), 0.f);
}
#endif


#if 0
// following: the same in opencl
/** The following is darktable Uniform Color Space 2022
 * © Aurélien Pierre
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/
 *
 * Use this space for color-grading in a perceptual framework.
 * The CAM terms have been removed for performance.
 **/

static inline float Y_to_dt_UCS_L_star(const float Y)
{
  // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = native_powr(Y, 0.631651345306265f);
  return 2.098883786377f * Y_hat / (Y_hat + 1.12426773749357f);
}

static inline float dt_UCS_L_star_to_Y(const float L_star)
{
  // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  return native_powr((1.12426773749357f * L_star / (2.098883786377f - L_star)), 1.5831518565279648f);
}

static inline void xyY_to_dt_UCS_UV(const float4 xyY, float UV_star_prime[2])
{
  float4 x_factors = { -0.783941002840055f,  0.745273540913283f, 0.318707282433486f, 0.f };
  float4 y_factors = {  0.277512987809202f, -0.205375866083878f, 2.16743692732158f,  0.f };
  float4 offsets   = {  0.153836578598858f, -0.165478376301988f, 0.291320554395942f, 0.f };

  float4 UVD = x_factors * xyY.x + y_factors * xyY.y + offsets;
  UVD.xy /= UVD.z;

  float UV_star[2] = { 0.f };
  const float factors[2]     = { 1.39656225667f, 1.4513954287f };
  const float half_values[2] = { 1.49217352929f, 1.52488637914f };
  for(int c = 0; c < 2; c++)
    UV_star[c] = factors[c] * ((float *)&UVD)[c] / (fabs(((float *)&UVD)[c]) + half_values[c]);

  // The following is equivalent to a 2D matrix product
  UV_star_prime[0] = -1.124983854323892f * UV_star[0] - 0.980483721769325f * UV_star[1];
  UV_star_prime[1] =  1.86323315098672f  * UV_star[0] + 1.971853092390862f * UV_star[1];
}


static inline float4 xyY_to_dt_UCS_JCH(const float4 xyY, const float L_white)
{
  /*
    input :
      * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
      * L_white the lightness of white as dt UCS L* lightness
      * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
              (background = middle grey, white = perfect diffuse white)
    range : xy in [0; 1], Y normalized for perfect diffuse white = 1
  */

  float UV_star_prime[2];
  xyY_to_dt_UCS_UV(xyY, UV_star_prime);

  const float L_star = Y_to_dt_UCS_L_star(xyY.z);
  const float M2 = UV_star_prime[0] * UV_star_prime[0] + UV_star_prime[1] * UV_star_prime[1]; // square of colorfulness M

  // should be JCH[0] = powf(L_star / L_white), cz) but we treat only the case where cz = 1
  float4 JCH;
  JCH.x = L_star / L_white;
  JCH.y = 15.932993652962535f * native_powr(L_star, 0.6523997524738018f) * native_powr(M2, 0.6007557017508491f) / L_white;
  JCH.z = atan2(UV_star_prime[1], UV_star_prime[0]);
  return JCH;

}

static inline float4 dt_UCS_JCH_to_xyY(const float4 JCH, const float L_white)
{
  /*
    input :
      * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
      * L_white the lightness of white as dt UCS L* lightness
      * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
              (background = middle grey, white = perfect diffuse white)
    range : xy in [0; 1], Y normalized for perfect diffuse white = 1
  */

  // should be L_star = powf(JCH[0], 1.f / cz) * L_white but we treat only the case where cz = 1
  const float L_star = JCH.x * L_white;
  const float M = native_powr(JCH.y * L_white / (15.932993652962535f * native_powr(L_star, 0.6523997524738018f)), 0.8322850678616855f);

  const float U_star_prime = M * native_cos(JCH.z);
  const float V_star_prime = M * native_sin(JCH.z);

  // The following is equivalent to a 2D matrix product
  const float UV_star[2] = { -5.037522385190711f * U_star_prime - 2.504856328185843f * V_star_prime,
                              4.760029407436461f * U_star_prime + 2.874012963239247f * V_star_prime };

  float UV[2] = {0.f};
  const float factors[2]     = { 1.39656225667f, 1.4513954287f };
  const float half_values[2] = { 1.49217352929f,1.52488637914f };
  for(int c = 0; c < 2; c++)
    UV[c] = -half_values[c] * UV_star[c] / (fabs(UV_star[c]) - factors[c]);

  const float4 U_factors = {  0.167171472114775f,   -0.150959086409163f,    0.940254742367256f,  0.f };
  const float4 V_factors = {  0.141299802443708f,   -0.155185060382272f,    1.000000000000000f,  0.f };
  const float4 offsets   = { -0.00801531300850582f, -0.00843312433578007f, -0.0256325967652889f, 0.f };

  float4 xyD = U_factors * UV[0] + V_factors * UV[1] + offsets;

  float4 xyY;
  xyY.x = xyD.x / xyD.z;
  xyY.y = xyD.y / xyD.z;
  xyY.z = dt_UCS_L_star_to_Y(L_star);
  return xyY;
}


static inline float4 dt_UCS_JCH_to_HSB(const float4 JCH)
{
  float4 HSB;
  HSB.z = JCH.x * (native_powr(JCH.y, 1.33654221029386f) + 1.f);
  HSB.y = (HSB.z > 0.f) ? JCH.y / HSB.z : 0.f;
  HSB.x = JCH.z;
  return HSB;
}


static inline float4 dt_UCS_HSB_to_JCH(const float4 HSB)
{
  float4 JCH;
  JCH.z = HSB.x;
  JCH.y = HSB.y * HSB.z;
  JCH.x = HSB.z / (native_powr(JCH.y, 1.33654221029386f) + 1.f);
  return JCH;
}


static inline float4 dt_UCS_JCH_to_HCB(const float4 JCH)
{
  float4 HCB;
  HCB.z = JCH.x * (native_powr(JCH.y, 1.33654221029386f) + 1.f);
  HCB.y = JCH.y;
  HCB.x = JCH.z;
  return HCB;
}


static inline float4 dt_UCS_HCB_to_JCH(const float4 HCB)
{
  float4 JCH;
  JCH.z = HCB.x;
  JCH.y = HCB.y;
  JCH.x = HCB.z / (native_powr(HCB.y, 1.33654221029386f) + 1.f);
  return JCH;
}
#endif
