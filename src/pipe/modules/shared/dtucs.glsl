/* The following is darktable Uniform Color Space 2022
 * © Aurélien Pierre
 * https://eng.aurelienpierre.com/2022/02/color-saturation-control-for-the-21th-century/
 *
 * Use this space for color-grading in a perceptual framework.
 * The CAM terms have been removed for performance.
 *
 * stolen from darktable, MIT licence
 */

float Y_to_dt_UCS_L_star(const float Y)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  const float Y_hat = pow(Y, 0.631651345306265);
  return 2.098883786377 * Y_hat / (Y_hat + 1.12426773749357);
}

float dt_UCS_L_star_to_Y(const float L_star)
{ // WARNING: L_star needs to be < 2.098883786377, meaning Y needs to be < 3.875766378407574e+19
  return pow((1.12426773749357 * L_star / (2.098883786377 - L_star)), 1.5831518565279648);
}

vec2 xy_to_dt_UCS_UV(vec2 xy)
{
  const mat3 M1 = mat3( // column vectors:
      vec3(-0.783941002840055,  0.745273540913283, 0.318707282433486),
      vec3( 0.277512987809202, -0.205375866083878, 2.16743692732158),
      vec3( 0.153836578598858, -0.165478376301988, 0.291320554395942));
  vec3 uvd = M1 * vec3(xy, 1.0);
  uvd.xy /= uvd.z;

  const vec2 factors     = vec2(1.39656225667, 1.4513954287 );
  const vec2 half_values = vec2(1.49217352929, 1.52488637914);
  vec2 UV_star = factors * uvd.xy / (abs(uvd.xy) + half_values);

  const mat2 M2 = mat2(-1.124983854323892, 1.86323315098672, - 0.980483721769325, + 1.971853092390862);
  return M2 * UV_star;
}

//  input :
//    * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
//    * L_white the lightness of white as dt UCS L* lightness
//    * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
//            (background = middle grey, white = perfect diffuse white)
//  range : xy in [0; 1], Y normalized for perfect diffuse white = 1
vec3 xyY_to_dt_UCS_JCH(const vec3 xyY, const float L_white)
{
  vec2 UV_star_prime = xy_to_dt_UCS_UV(xyY.xy);
  const float L_star = Y_to_dt_UCS_L_star(xyY.z);
  const float M2 = dot(UV_star_prime, UV_star_prime); // square of colorfulness M
  return vec3( // should be JCH[0] = powf(L_star / L_white), cz) but we treat only the case where cz = 1
      L_star / L_white,
      15.932993652962535 * pow(L_star, 0.6523997524738018) * pow(M2, 0.6007557017508491) / L_white,
      atan(UV_star_prime.y, UV_star_prime.x));
}

//  input :
//    * xyY in normalized CIE XYZ for the 2° 1931 observer adapted for D65
//    * L_white the lightness of white as dt UCS L* lightness
//    * cz = 1 for standard pre-print proofing conditions with average surround and n = 20 %
//            (background = middle grey, white = perfect diffuse white)
//  range : xy in [0; 1], Y normalized for perfect diffuse white = 1
vec3 dt_UCS_JCH_to_xyY(const vec3 JCH, const float L_white)
{
  const float L_star = JCH.x * L_white; // should be L_star = powf(JCH[0], 1.f / cz) * L_white but we treat only cz = 1
  const float M = pow(JCH.y * L_white / (15.932993652962535 * pow(L_star, 0.6523997524738018)), 0.8322850678616855);

  vec2 UV_star = M * vec2(cos(JCH.z), sin(JCH.z)); // uv*'
  const mat2 M1 = mat2(-5.037522385190711, 4.760029407436461, -2.504856328185843, 2.874012963239247); // col major
  UV_star = M1 * UV_star;

  const vec2 factors     = vec2(1.39656225667, 1.4513954287);
  const vec2 half_values = vec2(1.49217352929, 1.52488637914);
  vec2 UV = -half_values * UV_star / (abs(UV_star) - factors);

  const mat3 M2 = mat3( // given as column vectors
      vec3( 0.167171472114775,   -0.150959086409163,    0.940254742367256),
      vec3( 0.141299802443708,   -0.155185060382272,    1.000000000000000),
      vec3(-0.00801531300850582, -0.00843312433578007, -0.0256325967652889));
  vec3 xyD = M2 * vec3(UV, 1.0);
  return vec3(xyD.xy / xyD.z, dt_UCS_L_star_to_Y(L_star));
}

vec3 dt_UCS_JCH_to_HSB(const vec3 JCH)
{
  vec3 HSB;
  HSB.z = JCH.x * (pow(JCH.y, 1.33654221029386) + 1.);
  return vec3(JCH.z, (HSB.z > 0.) ? JCH.y / HSB.z : 0., HSB.z);
}

vec3 dt_UCS_HSB_to_JCH(const vec3 HSB)
{
  return vec3(HSB.z / (pow(HSB.y*HSB.z, 1.33654221029386) + 1.), HSB.y * HSB.z, HSB.x);
}

vec3 dt_UCS_JCH_to_HCB(const vec3 JCH)
{
  return vec3(JCH.zy, JCH.x * (pow(JCH.y, 1.33654221029386) + 1.));
}

vec3 dt_UCS_HCB_to_JCH(const vec3 HCB)
{
  return vec3(HCB.z / (pow(HCB.y, 1.33654221029386) + 1.), HCB.y, HCB.x);
}
