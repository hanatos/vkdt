#include "localsize.h"
#define M_PI   3.14159265358979323846

float clamps(float v, float m, float M)
{ // opposite to the definition of clamp this one never passes on v in case of NaN (but clamps to M instead)
  return max(m, min(M, v));
}

vec4 sample_catmull_rom_1d(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  float samplePos = uv.x * texSize.x;
  float texPos1 = floor(samplePos - 0.5) + 0.5;

  float f = samplePos - texPos1;

  float w0 = f * ( -0.5 + f * (1.0 - 0.5*f));
  float w1 = 1.0 + f * f * (-2.5 + 1.5*f);
  float w2 = f * ( 0.5 + f * (2.0 - 1.5*f) );
  float w3 = f * f * (-0.5 + 0.5 * f);

  float w12 = w1 + w2;
  float offset12 = w2 / (w1 + w2);

  // Compute the final UV coordinates we'll use for sampling the texture
  float texPos0 = texPos1 - 1.0;
  float texPos3 = texPos1 + 2.0;
  float texPos12 = texPos1 + offset12;

  texPos0 /= texSize.x;
  texPos3 /= texSize.x;
  texPos12 /= texSize.x;

  vec4 result = vec4(0.0);
  result += textureLod(tex, vec2(texPos0.x,  uv.y),  0) * w0.x;
  result += textureLod(tex, vec2(texPos12.x, uv.y),  0) * w12.x;
  result += textureLod(tex, vec2(texPos3.x,  uv.y),  0) * w3.x;
  return result;
}


// http://vec3.ca/bicubic-filtering-in-fewer-taps/
vec4 sample_catmull_rom(sampler2D tex, vec2 uv)
{
  // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
  // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
  // location [1, 1] in the grid, where [0, 0] is the top left corner.
  vec2 texSize = textureSize(tex, 0);
  vec2 samplePos = uv * texSize;
  vec2 texPos1 = floor(samplePos - 0.5) + 0.5;

  // Compute the fractional offset from our starting texel to our original sample location, which we'll
  // feed into the Catmull-Rom spline function to get our filter weights.
  vec2 f = samplePos - texPos1;

  // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
  // These equations are pre-expanded based on our knowledge of where the texels will be located,
  // which lets us avoid having to evaluate a piece-wise function.
  vec2 w0 = f * ( -0.5 + f * (1.0 - 0.5*f));
  vec2 w1 = 1.0 + f * f * (-2.5 + 1.5*f);
  vec2 w2 = f * ( 0.5 + f * (2.0 - 1.5*f) );
  vec2 w3 = f * f * (-0.5 + 0.5 * f);

  // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
  // simultaneously evaluate the middle 2 samples from the 4x4 grid.
  vec2 w12 = w1 + w2;
  vec2 offset12 = w2 / (w1 + w2);

  // Compute the final UV coordinates we'll use for sampling the texture
  vec2 texPos0 = texPos1 - vec2(1.0);
  vec2 texPos3 = texPos1 + vec2(2.0);
  vec2 texPos12 = texPos1 + offset12;

  texPos0 /= texSize;
  texPos3 /= texSize;
  texPos12 /= texSize;

  vec4 result = vec4(0.0);
  result += textureLod(tex, vec2(texPos0.x,  texPos0.y),  0) * w0.x * w0.y;
  result += textureLod(tex, vec2(texPos12.x, texPos0.y),  0) * w12.x * w0.y;
  result += textureLod(tex, vec2(texPos3.x,  texPos0.y),  0) * w3.x * w0.y;

  result += textureLod(tex, vec2(texPos0.x,  texPos12.y), 0) * w0.x * w12.y;
  result += textureLod(tex, vec2(texPos12.x, texPos12.y), 0) * w12.x * w12.y;
  result += textureLod(tex, vec2(texPos3.x,  texPos12.y), 0) * w3.x * w12.y;

  result += textureLod(tex, vec2(texPos0.x,  texPos3.y),  0) * w0.x * w3.y;
  result += textureLod(tex, vec2(texPos12.x, texPos3.y),  0) * w12.x * w3.y;
  result += textureLod(tex, vec2(texPos3.x,  texPos3.y),  0) * w3.x * w3.y;

  return result;
}

// cannibalised version of the above, softer kernel:
vec4 sample_soft(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 samplePos = uv * texSize;
  vec2 texPos1 = samplePos;

  vec2 texPos0 = texPos1 - vec2(1.5);
  vec2 texPos3 = texPos1 + vec2(1.5);
  vec2 texPos12 = texPos1;

  texPos0 /= texSize;
  texPos3 /= texSize;
  texPos12 /= texSize;

  vec4 result = vec4(0.0);
  result += textureLod(tex, vec2(texPos0.x,  texPos0.y),  0);
  result += textureLod(tex, vec2(texPos12.x, texPos0.y),  0);
  result += textureLod(tex, vec2(texPos3.x,  texPos0.y),  0);

  result += textureLod(tex, vec2(texPos0.x,  texPos12.y), 0);
  result += textureLod(tex, vec2(texPos12.x, texPos12.y), 0);
  result += textureLod(tex, vec2(texPos3.x,  texPos12.y), 0);

  result += textureLod(tex, vec2(texPos0.x,  texPos3.y),  0);
  result += textureLod(tex, vec2(texPos12.x, texPos3.y),  0);
  result += textureLod(tex, vec2(texPos3.x,  texPos3.y),  0);

  return result / 9.0f;
}

// use 2x2 lookups to simulate 3x3
vec4 sample_semisoft(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 texPosc = uv * texSize;

  vec2 texPos0 = texPosc - vec2(.5);
  vec2 texPos1 = texPosc + vec2(.5);

  texPos0 /= texSize;
  texPos1 /= texSize;

  vec4 result = vec4(0.0);
  result += textureLod(tex, vec2(texPos0.x, texPos0.y), 0);
  result += textureLod(tex, vec2(texPos1.x, texPos0.y), 0);
  result += textureLod(tex, vec2(texPos0.x, texPos1.y), 0);
  result += textureLod(tex, vec2(texPos1.x, texPos1.y), 0);

  return result / 4.0f;
}

float luminance_rec2020(vec3 rec2020)
{
  // excerpt from the rec2020 to xyz matrix (y channel only)
  return dot(vec3(2.62700212e-01, 6.77998072e-01, 5.93017165e-02), rec2020);
}

// full 4-wide version, passing through all channels:
vec4 sample_flower4(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 tc = uv * texSize;

  vec2 off0 = vec2( 1.2, 0.4);
  vec2 off1 = vec2(-0.4, 1.2);

  const float t = 36.0/256.0;
  vec4 result = vec4(0);
#define LOAD(T, W)\
  do {\
    vec4 v = textureLod(tex, T, 0);\
    result += W * v;\
  } while(false)
  LOAD(uv, t);
  LOAD((tc+off0)/texSize, (1.0-t)/4.0);
  LOAD((tc-off0)/texSize, (1.0-t)/4.0);
  LOAD((tc+off1)/texSize, (1.0-t)/4.0);
  LOAD((tc-off1)/texSize, (1.0-t)/4.0);
  return result;
}

vec4 sample_flower_adj4(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 tc = uv * texSize;

  vec2 off0 = vec2( 1.2, -0.4);
  vec2 off1 = vec2( 0.4,  1.2);

  const float t = 36.0/256.0;
  vec4 result = vec4(0);
  LOAD(uv, t);
  LOAD((tc+off0)/texSize, (1.0-t)/4.0);
  LOAD((tc-off0)/texSize, (1.0-t)/4.0);
  LOAD((tc+off1)/texSize, (1.0-t)/4.0);
  LOAD((tc-off1)/texSize, (1.0-t)/4.0);
#undef LOAD
  return result;
}
// almost 5x5 support in 5 taps
vec4 sample_flower(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 tc = uv * texSize;

  vec2 off0 = vec2( 1.2, 0.4);
  vec2 off1 = vec2(-0.4, 1.2);

  const float t = 36.0/256.0;
  vec4 result = vec4(0);
#define LOAD(T, W)\
  do {\
    vec4 v = texture(tex, T);\
    float l = max(v.r, max(v.g, v.b));\
    result += vec4(W*v.rgb, W*l*l);\
  } while(false)
  LOAD(uv, t);
  LOAD((tc+off0)/texSize, (1.0-t)/4.0);
  LOAD((tc-off0)/texSize, (1.0-t)/4.0);
  LOAD((tc+off1)/texSize, (1.0-t)/4.0);
  LOAD((tc-off1)/texSize, (1.0-t)/4.0);
  return result;
}

// flower rotated the other way around to even out anisotropic looks:
vec4 sample_flower_adj(sampler2D tex, vec2 uv)
{
  vec2 texSize = textureSize(tex, 0);
  vec2 tc = uv * texSize;

  vec2 off0 = vec2( 1.2, -0.4);
  vec2 off1 = vec2( 0.4,  1.2);

  const float t = 36.0/256.0;
  vec4 result = vec4(0);
  LOAD(uv, t);
  LOAD((tc+off0)/texSize, (1.0-t)/4.0);
  LOAD((tc-off0)/texSize, (1.0-t)/4.0);
  LOAD((tc+off1)/texSize, (1.0-t)/4.0);
  LOAD((tc-off1)/texSize, (1.0-t)/4.0);
#undef LOAD
  return result;
}

// (c) christoph peters:
void evd2x2(
    out vec2 eval,
    out vec2 evec0,
    out vec2 evec1,
    mat2 M)
{
	// Define some short hands for the matrix entries
	float a = M[0][0];
	float b = M[1][0];
	float c = M[1][1];
	// Compute coefficients of the characteristic polynomial
	float pHalf = -0.5 * (a + c);
	float q = a*c - b*b;
	// Solve the quadratic
	float discriminant_root = sqrt(pHalf * pHalf - q);
	eval.x = -pHalf + discriminant_root;
	eval.y = -pHalf - discriminant_root;
	// Subtract a scaled identity matrix to obtain a rank one matrix
	float a0 = a - eval.x;
	float b0 = b;
	float c0 = c - eval.x;
	// The column space of this matrix is orthogonal to the first eigenvector 
	// and since the eigenvectors are orthogonal themselves, it agrees with the 
	// second eigenvector. Pick the longer column to avoid cancellation.
	float squaredLength0 = a0*a0 + b0*b0;
	float squaredLength1 = b0*b0 + c0*c0;
	float squaredLength;
	if (squaredLength0 > squaredLength1)
  {
		evec1.x = a0;
		evec1.y = b0;
		squaredLength = squaredLength0;
	}
	else {
		evec1.x = b0;
		evec1.y = c0;
		squaredLength = squaredLength1;
	}
	// If the eigenvector is exactly zero, both eigenvalues are the same and the 
	// choice of orthogonal eigenvectors is arbitrary
	evec1.x = (squaredLength == 0.0) ? 1.0 : evec1.x;
	squaredLength = (squaredLength == 0.0) ? 1.0 : squaredLength;
	// Now normalize
	float invLength = 1.0 / sqrt(squaredLength);
	evec1.x *= invLength;
	evec1.y *= invLength;
	// And rotate to get the other eigenvector
	evec0.x =  evec1.y;
	evec0.y = -evec1.x;
}

// returns true if the chromaticity coordinates given here
// lie outside an analytic approximation to the spectral locus
bool outside_spectral_locus(vec2 xy)
{
  if(xy.x + xy.y > 1 || xy.x < 0) return true;
  if(xy.y < (xy.x-0.17)*0.47) return true;
  // turingbot ftw!
  if(xy.y > pow(tanh(5.73314*xy.x),0.10809)-xy.x) return true;
  // if(xy.y > 1.02942-((0.00585019/(0.014045+0.508182*xy.x))+1.01853*xy.x)) return true;
  if(xy.x < 0.18 && 
     xy.y < (1.14195-((6.57178+(-5.63051*xy.x))*xy.x))*(0.443525-2.58406*xy.x+(0.000566481/xy.x))+0.00587683) 
    return true;

  return false;
}

// use the colour reconstruction proposed in Mantiuk et al. "Color correction
// for tone mapping" (EG 2009):
// instead of simple RGB scaling or Lab magic, they propose a sigmoid function
// that saturates colour saturation near black and white on the output.
/*
  RGBout = (RGBin/Lin)^s Lout               (2)
  RGBout = ((RGBin/Lin - 1)*s + 1) Lout     (3)
  pick one of (2) or (3), (3) is better for exact brightness, but introduces
  more severe hue shifts.
  assume tone curve would be something like
  Lout = (Lin * b)^c                        (4)
  find s(c) as
  s(c) = (1+k1) c^k2 / (1 + k1 c^k2)        (5)
  they find c for non contrast curves as in (4) by using log/log space:
  c(Lin) = d/dlogLin f(logLin)              (6)
  what if we instead approximate the local surroundings of Lin/Lout by
  a contrast curve of shape (4)?
  i.e.
  c = log(Lout)/log(b*Lin)                  (O1)
  probably assuming exposure correction b = 1.
  so:
  a) compute Lout from Lin as we do
  b) compute c by eq (O1)
  c) compute s(c) by eq (5)
  d) compute RGB by eq (2) or (3), to taste
*/
vec3 adjust_colour(vec3 rgb, float yo, float y)
{
// #define EQ2 // this makes very tiny differences in my tests, but it can result in quite disturbing brightness changes even for yo == y
  float eps = 1e-8;
  // we have to deal with the contrast curve changing sign > 1.0.
  // this can be done various ways. just clamping or evaluating the logarithm of 1+x.
  // log(1+x) looks really very good on landscapes, log(clamp(x)) looks better on
  // much increased blacks for portraits. possibly might tweak this in the future
  // and potentially need to expose control over this in params :(
  // this one desaturates deep shadows unacceptably for portraits
  // float c = clamp(log(1.0+max(y, 0.0))/(log(1.0+max(yo, 0.0)) + eps), 0, 4.0); // excellent for landsc
  // those two work great on portraits (i see no diff):
  // the 0.6 safeguards the sharp transition to complete desaturation when the two logs change sign.
  float c = clamp(log(clamp(y*0.6, eps, 1.0))/(log(clamp(yo*0.6, eps, 1.0)) + eps), 0, 4.0);
#ifdef EQ2
  float k1 = 1.6774, k2 = .9925;  // params for eq (2)
#else
  float k1 = 2.3892, k2 = 0.8552; // params for eq (3)
#endif
  float s = (1.0+k1) * pow(c, k2) / (1.0 + k1*pow(c, k2));
  s = clamp(s, eps, 1.0-eps);
#ifdef EQ2
  // eq (2) preserving colours, not brightness:
  // want to avoid saturating dark noise, hence fairly big epsilon 1e-2 here
  return pow(rgb/(yo+1e-2), vec3(s)) * y; // eq (2)
#undef EQ2
#else
  return ((rgb / (yo+eps) - 1.0)*s + 1.0) * y;  // eq (3)
#endif
}

// apparently this is what the dng spec says should be used to correct colour
// after application of tone-changing operators (curve). with all the sorting,
// it's based on hsv and designed to keep hue constant in that space.
vec3 // returns hue mapped rgb colour
adjust_colour_dng(
    vec3 col0, // original colour
    vec3 col1) // mapped colour, for instance by rgb curves
{
  bvec3 flip = bvec3(false); // bubble sort
  if(col0.b > col0.g) { col0.bg = col0.gb; col1.bg = col1.gb; flip.x = true; }
  if(col0.g > col0.r) { col0.rg = col0.gr; col1.rg = col1.gr; flip.y = true; }
  if(col0.b > col0.g) { col0.bg = col0.gb; col1.bg = col1.gb; flip.z = true; }

  col1.g = mix(col1.b, col1.r, (col0.g-col0.b+1e-6)/(col0.r-col0.b+1e-6));

  if(flip.z) col1.bg = col1.gb;
  if(flip.y) col1.rg = col1.gr;
  if(flip.x) col1.bg = col1.gb;
  return col1;
}

float mrand(inout uint seed)
{ // Algorithm "xor" from p. 4 of Marsaglia, "Xorshift RNGs"
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  // do it the rust way, results in more distinct numbers in [0,1) than pasting the mantissa bits to 0x3f800000:
  return (seed >> 8) * 5.9604644775390625e-08;
}

vec2 warp_gaussian(vec2 xi)
{ // warp two uniform [0,1) random variables to two standard normal distributed ones (box muller transform)
  // use 1-x instead of x to avoid log(0)
  return sqrt(-2.0*log(1.0-xi.x))*vec2(cos(2.0*M_PI*xi.y), sin(2.0*M_PI*xi.y));
}
