#include "localsize.h"

struct roi_t
{
  uvec2 full;      // full input size
  uvec2 roi;       // dimensions of region of interest
  ivec2 off;       // offset in full image
  float scale;     // wd * scale is on input scale
  float pad0;      // alignment of structs will be a multiple of vec4 it seems :(
  // so we pad explicitly for sanity of mind.
  // alternatively we could specify layout(offset=48) etc below.
};

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
  vec3 w = vec3(0.2126729, 0.7151522, 0.0721750);
  return dot(w, rec2020);
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

