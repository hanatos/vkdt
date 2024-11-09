/*!	Returns the complex conjugate of the given complex number (i.e. it changes 
	the sign of the y-component).*/
vec2 complex_conj(vec2 z)
{
	return vec2(z.x,-z.y);
}
/*!	Component-wise sign flip.*/
vec2 complex_neg(vec2 z)
{
	return vec2(-z.x,-z.y);
}
/*!	This function implements complex addition.*/
vec2 complex_add(vec2 lhs,vec2 rhs)
{
	return vec2(lhs.x+rhs.x,lhs.y+rhs.y);
}
/*!	This function implements complex subtraction.*/
vec2 complex_sub(vec2 lhs,vec2 rhs)
{
	return vec2(lhs.x-rhs.x,lhs.y-rhs.y);
}
/*!	This function implements complex multiplication.*/
vec2 complex_mul(vec2 lhs,vec2 rhs)
{
	return vec2(lhs.x*rhs.x-lhs.y*rhs.y,lhs.x*rhs.y+lhs.y*rhs.x);
}
/*!	This function implements mixed real complex addition.*/
vec2 complex_add_mixed(float lhs,vec2 rhs)
{
	return vec2(lhs+rhs.x,rhs.y);
}
/*!	This function implements  mixed real complex multiplication.*/
vec2 complex_mul_mixed(float lhs,vec2 rhs)
{
	return vec2(lhs*rhs.x,lhs*rhs.y);
}
/*!	This function computes the squared magnitude of the given complex number.*/
float complex_abs_sqr(vec2 z)
{
	return z.x*z.x+z.y*z.y;
}
/*!	This function computes the squared magnitude of the given real number.*/
float complex_sqr(float z)
{
	return z*z;
}
/*!	This function computes the quotient of two complex numbers. The denominator 
	must not be zero.*/
vec2 complex_div(vec2 numerator,vec2 denominator)
{
	float factor=1.0/complex_abs_sqr(denominator);
	return vec2((numerator.x*denominator.x+numerator.y*denominator.y)*factor,(-numerator.x*denominator.y+numerator.y*denominator.x)*factor);
}
/*!	This function implements computation of the reciprocal of the given 
	non-zero complex number.*/
vec2 complex_rcp(vec2 z)
{
	float factor=1.0/complex_abs_sqr(z);
	return vec2(z.x*factor,-z.y*factor);
}

// return euler's formula exp(ix) = cos(x) + i sin(x)
vec2 complex_euler(float x)
{
  return vec2(cos(x), sin(x));
}

vec2 complex_sqrt(vec2 z)
{
  float r = sqrt(complex_abs_sqr(z));
  return complex_mul_mixed(sqrt(r), complex_add_mixed(r, z) /
      sqrt(complex_abs_sqr(complex_add_mixed(r, z))));
}

/*! Implements an approximation to arctan with a maximal absolute error around 
	1.1e-5f.*/
float complex_fast_atan(float tan)
{
	bool negative=(tan<0.0);
	float abs_tan=negative?(-tan):tan;
	bool greater_one=(abs_tan>1.0);
	float x=greater_one?abs_tan:1.0;
	x=1.0/x;
	float y=greater_one?1.0:abs_tan;
	x=x*y;
	y=x*x;
	float z=y*0.020835-0.085133;
	z=y*z+0.180141;
	z=y*z-0.330299;
	y=y*z+0.999866;
	z=y*x;
	z=z*-2.0+1.570796;
	z=greater_one?z:0.0;
	x=x*y+z;
	return negative?(-x):x;
}
