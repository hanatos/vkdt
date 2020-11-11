#define M_PI 3.1415926535897932384626433832795
#define INV_SQRT_PI 0.56418958354

float fsqrt(float val)
{
  return max(0, sqrt(val));
}

// https://github.com/mitsuba-renderer/mitsuba/blob/1fd0f671dfcb77f813c0d6a36f2aa4e480b5ca8e/src/libcore/math.cpp#L23
float erfinv(float x) {
    // Based on "Approximating the erfinv function" by Mark Giles
    float w = -log(max((1.0 - x)*(1.0 + x), 1e-10));
    float p;
    if (w <  5) {
        w = w -  2.5;
        p =  2.81022636e-08;
        p =  3.43273939e-07 + p*w;
        p =  -3.5233877e-06 + p*w;
        p =  -4.39150654e-06 + p*w;
        p =  0.00021858087 + p*w;
        p =  -0.00125372503 + p*w;
        p =  -0.00417768164 + p*w;
        p =  0.246640727 + p*w;
        p =  1.50140941 + p*w;
    } else {
        w = sqrt(w) -  3;
        p =  -0.000200214257;
        p =  0.000100950558 + p*w;
        p =  0.00134934322 + p*w;
        p =  -0.00367342844 + p*w;
        p =  0.00573950773 + p*w;
        p =  -0.0076224613 + p*w;
        p =  0.00943887047 + p*w;
        p =  1.00167406 + p*w;
        p =  2.83297682 + p*w;
    }
    return p*x;
}

float erf(float x) {
    float a1 =   0.254829592;
    float a2 =  -0.284496736;
    float a3 =   1.421413741;
    float a4 =  -1.453152027;
    float a5 =   1.061405429;
    float p  =   0.3275911;

    // Save the sign of x
    float sign = sign(x);
    x = abs(x);

    // A&S formula 7.1.26
    float t =  1.0 / ( 1.0 + p*x);
    float y =  1.0 - (((((a5*t + a4)*t) + a3)*t + a2)*t + a1)*t*exp(-x*x);

    return sign*y;
}

// returns TBN in column order
mat3 getONB(const in vec3 n )
{
  mat3 tbn; //tangent bitangent normal

  const vec3 up = vec3(0.0, 1.0, 0.0);
  tbn[0] = normalize(cross(up,n));
  tbn[1] = normalize(cross(n,tbn[0]));
  tbn[2] = n;

  // glsl has column major ordering
  // which basically works as a transpose
  return tbn;
}

// (c) christoph peters:
void evd2x2_mod(
    out vec2 eval,
    out vec2 evec0,
    out vec2 evec1,
    mat2 M)
{
	// Define some short hands for the matrix entries
	float a = M[0].x;
	float b = M[1].x;
	float c = M[1].y;
	// Compute coefficients of the characteristic polynomial
	float pHalf = -0.5 * (a + c);
	float q = a*c - b*b;
	// Solve the quadratic
	float discriminant_root = fsqrt(pHalf * pHalf - q);
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
		evec1 = vec2(a0, b0);
		squaredLength = squaredLength0;
	}
	else {
		evec1 = vec2(b0, c0);
		squaredLength = squaredLength1;
	}
	// If the eigenvector is exactly zero, both eigenvalues are the same and the 
	// choice of orthogonal eigenvectors is arbitrary
  // set second eigenvector to [0, 1] if needed or normalize
  evec1 = (squaredLength == 0) ? vec2(0, 1.0) : evec1 / fsqrt(squaredLength);
	// And rotate to get the other eigenvector
	evec0.x =  evec1.y;
	evec0.y = -evec1.x;
}