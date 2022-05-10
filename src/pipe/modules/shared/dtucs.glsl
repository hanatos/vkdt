/* The following is darktable Uniform Color Space 2022
 * © Aurélien Pierre
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/
 *
 * Use this space for color-grading in a perceptual framework.
 * The CAM terms have been removed for performance.
 *
 * stolen from darktable, GPLv3
 */

float Y_to_dt_UCS_L_star(const float Y)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = pow(Y, 0.631651345306265f);
  return 2.098883786377f * Y_hat / (Y_hat + 1.12426773749357f);
}

float dt_UCS_L_star_to_Y(const float L_star)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
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
