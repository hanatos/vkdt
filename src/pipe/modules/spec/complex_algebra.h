/*! \file
	This header defines utility functions to deal with complex numbers.*/

// A type for complex numbers that is not float complex
typedef struct{
	// Real part and imaginary part
	float x,y;
} float_complex;


/*!	Returns the complex conjugate of the given complex number (i.e. it changes 
	the sign of the y-component).*/
inline float_complex conjugate(float_complex z){
	return (float_complex){z.x,-z.y};
}
/*!	Component-wise sign flip.*/
inline float_complex negate(float_complex z){
	return (float_complex){-z.x,-z.y};
}
/*!	This function implements complex addition.*/
inline float_complex add(float_complex lhs,float_complex rhs){
	return (float_complex){lhs.x+rhs.x,lhs.y+rhs.y};
}
/*!	This function implements complex subtraction.*/
inline float_complex subtract(float_complex lhs,float_complex rhs){
	return (float_complex){lhs.x-rhs.x,lhs.y-rhs.y};
}
/*!	This function implements complex multiplication.*/
inline float_complex multiply(float_complex lhs,float_complex rhs){
	return (float_complex){lhs.x*rhs.x-lhs.y*rhs.y,lhs.x*rhs.y+lhs.y*rhs.x};
}
/*!	This function implements mixed real complex addition.*/
inline float_complex add_mixed(float lhs,float_complex rhs){
	return (float_complex){lhs+rhs.x,rhs.y};
}
/*!	This function implements  mixed real complex multiplication.*/
inline float_complex multiply_mixed(float lhs,float_complex rhs){
	return (float_complex){lhs*rhs.x,lhs*rhs.y};
}
/*!	This function computes the squared magnitude of the given complex number.*/
inline float absSqr(float_complex z){
	return z.x*z.x+z.y*z.y;
}
/*!	This function computes the squared magnitude of the given complex number.*/
inline float sqr(float z){
	return z*z;
}
/*!	This function computes the quotient of two complex numbers. The denominator 
	must not be zero.*/
inline float_complex divide(float_complex numerator,float_complex denominator){
	float factor=1.0f/absSqr(denominator);
	return (float_complex){(numerator.x*denominator.x+numerator.y*denominator.y)*factor,(-numerator.x*denominator.y+numerator.y*denominator.x)*factor};
}
/*!	This function implements computation of the reciprocal of the given 
	non-zero complex number.*/
inline float_complex reciprocal(float_complex z){
	float factor=1.0f/absSqr(z);
	return (float_complex){z.x*factor,-z.y*factor};
}

/*! Implements an approximation to arctan with a maximal absolute error around 
	1.1e-5f.*/
inline float fast_atan(float tan){
	int negative=(tan<0.0f);
	float abs_tan=negative?(-tan):tan;
	int greater_one=(abs_tan>1.0f);
	float x=greater_one?abs_tan:1.0f;
	x=1.0f/x;
	float y=greater_one?1.0f:abs_tan;
	x=x*y;
	y=x*x;
	float z=y*0.020835f-0.085133f;
	z=y*z+0.180141f;
	z=y*z-0.330299f;
	y=y*z+0.999866f;
	z=y*x;
	z=z*-2.0f+1.570796f;
	z=greater_one?z:0.0f;
	x=x*y+z;
	return negative?(-x):x;
}
