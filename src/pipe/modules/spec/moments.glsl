#include "complex.glsl"

float lambda_to_phase(float wavelength)
{
  float x = 0.2f*(wavelength-360.0f);
  return smoothstep(21, 60, x) * (3.141592653589793-0.9) - 3.141592653589793 + smoothstep(10,27,x)*0.9;
}

// returns true iff the Toeplitz matrix associated with the given exponential
// moments is positive definite. The approach is based on Levinson's algorithm
// to ensure that the result is equivalent to the result of the reconstruction
// code even in presence of rounding errors.
bool valid(const vec2 pMoment[3])
{
  vec2 pOutSolution[2];
  if (pMoment[0].x <= 0.0) return false;
  pOutSolution[0] = vec2(1.0 / pMoment[0].x, 0.0);
  vec2 dotProduct;
  vec2 flippedSolution1;
  vec2 flippedSolution2;
  float factor;
  dotProduct = pOutSolution[0].x * pMoment[1];
  factor = 1.0 / (1.0 - absSqr(dotProduct));
  if (factor <= 0.0) return false;
  flippedSolution1 = vec2(pOutSolution[0].x, 0.0);
  pOutSolution[0] = vec2(factor*pOutSolution[0].x, 0.0);
  pOutSolution[1] = factor * (-flippedSolution1.x * dotProduct);
  dotProduct = pOutSolution[0].x * pMoment[2] + pOutSolution[1] * pMoment[1];
  factor = 1.0 / (1.0 - absSqr(dotProduct));
  if (factor <= 0.0) return false;
  return true;
}

void trigonometricToExponentialMomentsReal3(out vec2 pOutExponentialMoment[3],const vec3 pTrigonometricMoment){
	float zerothMomentPhase=3.14159265*pTrigonometricMoment.x-1.57079633;
	pOutExponentialMoment[0]=vec2(cos(zerothMomentPhase),sin(zerothMomentPhase));
	pOutExponentialMoment[0]=multiply_mixed(0.0795774715,pOutExponentialMoment[0]);
	pOutExponentialMoment[1]=multiply_mixed(pTrigonometricMoment.y,multiply(vec2(0.0,6.28318531),pOutExponentialMoment[0]));
	pOutExponentialMoment[2]=add(multiply_mixed(pTrigonometricMoment.z,multiply(vec2(0.0,6.28318531),pOutExponentialMoment[0])),multiply_mixed(pTrigonometricMoment.y,multiply(vec2(0.0,3.14159265),pOutExponentialMoment[1])));
	pOutExponentialMoment[0]=multiply_mixed(2.0,pOutExponentialMoment[0]);
}

void levinsonsAlgorithm3(out vec2 pOutSolution[3], inout vec2 pFirstColumn[3])
{
	pOutSolution[0]=vec2(1.0/(pFirstColumn[0].x),0.0);
	vec2 dotProduct;
  float dotProductAbsSquare;
	vec2 flippedSolution1;
	vec2 flippedSolution2;
	float factor;
	dotProduct=multiply_mixed(pOutSolution[0].x,pFirstColumn[1]);
  // biasing
  dotProductAbsSquare = absSqr(dotProduct);
	factor=1.0/(1.0-dotProductAbsSquare);
#if 0
  float onemeps = 0.999;
  float corrf = 1.0/(1.0-onemeps*onemeps);
  if(factor < 0.0)
  {
    dotProduct = multiply_mixed(onemeps*1.0/sqrt(dotProductAbsSquare), dotProduct);
    pFirstColumn[1] = multiply_mixed(1.0/pOutSolution[0].x, dotProduct);
    factor = corrf;
    // onemeps = 0.0;
    // corrf = 1.0;
  }
#endif
	flippedSolution1=vec2(pOutSolution[0].x,0.0);
	pOutSolution[0]=vec2(factor*pOutSolution[0].x,0.0);
	pOutSolution[1]=multiply_mixed(factor,(negate(multiply_mixed(flippedSolution1.x, dotProduct))));
  vec2 unnormalised_next_moment_center = multiply(pOutSolution[1], pFirstColumn[1]);
	dotProduct = add(multiply_mixed(pOutSolution[0].x,pFirstColumn[2]), unnormalised_next_moment_center);
  // biasing
  dotProductAbsSquare = absSqr(dotProduct);
	factor=1.0/(1.0-dotProductAbsSquare);
#if 0
  if(factor < 0.0)
  {
    dotProduct = multiply_mixed(onemeps*1.0/sqrt(dotProductAbsSquare), dotProduct);
    pFirstColumn[2] = multiply_mixed(1.0/pOutSolution[0].x, subtract(dotProduct, unnormalised_next_moment_center));
    factor = corrf;
  }
#endif
	flippedSolution1=conjugate(pOutSolution[1]);
	flippedSolution2=vec2(pOutSolution[0].x,0.0);
	pOutSolution[0]=vec2(factor*pOutSolution[0].x,0.0);
	pOutSolution[1]=multiply_mixed(factor,(subtract(pOutSolution[1],multiply(flippedSolution1,dotProduct))));
	pOutSolution[2]=multiply_mixed(factor,(negate(multiply_mixed(flippedSolution2.x,dotProduct))));
}

vec2 evaluateFastHerglotzTransform3(const vec2 circlePoint,const vec2 pExponentialMoment[3],const vec2 pEvaluationPolynomial[3]){
	vec2 conjCirclePoint=conjugate(circlePoint);
	float polynomial2=pEvaluationPolynomial[0].x;
	vec2 polynomial1=add(pEvaluationPolynomial[1],multiply_mixed(polynomial2,conjCirclePoint));
	vec2 polynomial0=add(pEvaluationPolynomial[2],multiply(conjCirclePoint,polynomial1));
	vec2 dotProduct=add(multiply(polynomial1,pExponentialMoment[1]),multiply_mixed(polynomial2,pExponentialMoment[2]));
	return add(pExponentialMoment[0],multiply_mixed(2.0,divide(dotProduct,polynomial0)));
}

void prepareReflectanceSpectrumReal3(out vec2 pOutExponentialMoment[3], out vec2 pOutEvaluationPolynomial[3],const vec3 pTrigonometricMoment){
	trigonometricToExponentialMomentsReal3(pOutExponentialMoment,pTrigonometricMoment);
	levinsonsAlgorithm3(pOutEvaluationPolynomial,pOutExponentialMoment);
	pOutEvaluationPolynomial[0]=multiply_mixed(6.28318531,pOutEvaluationPolynomial[0]);
	pOutEvaluationPolynomial[1]=multiply_mixed(6.28318531,pOutEvaluationPolynomial[1]);
	pOutEvaluationPolynomial[2]=multiply_mixed(6.28318531,pOutEvaluationPolynomial[2]);
}

float evaluateReflectanceSpectrum3(const float phase,const vec2 pExponentialMoment[3],const vec2 pEvaluationPolynomial[3]){
	vec2 circlePoint;
  // could compute this at reduced precision, our wavelength is ~5nm anyways
	circlePoint=vec2(cos(phase),sin(phase));
	vec2 herglotzTransform;
	herglotzTransform=evaluateFastHerglotzTransform3(circlePoint,pExponentialMoment,pEvaluationPolynomial);
	return fast_atan(herglotzTransform.y/herglotzTransform.x)*0.318309886+0.500000000;
}



#if 0
/// lagrangian stuff here:

// helpers

float evaluateRealFourierSeries3(const vec2 circlePoint,const float pFourierCoefficient[3]){
	// Minimize sequential dependencies by computing powers of circlePoint
	// by multiplying previous powers by powers of two
	vec2 circlePointPower1=circlePoint;
	vec2 circlePointPower2=multiply(circlePointPower1,circlePointPower1);
	float result=pFourierCoefficient[1]*circlePointPower1.x+pFourierCoefficient[2]*circlePointPower2.x;
	return 2.0f*result+pFourierCoefficient[0];
}

void trigonometricToExponentialMoments3(vec2 pOutExponentialMoment[3],const vec2 pTrigonometricMoment[3]){
	float zerothMomentPhase=3.14159265f*pTrigonometricMoment[0].x-1.57079633f;
	pOutExponentialMoment[0]=(vec2){cosf(zerothMomentPhase),sinf(zerothMomentPhase)};
	pOutExponentialMoment[0]=multiply_mixed(0.0795774715f,pOutExponentialMoment[0]);
	pOutExponentialMoment[1]=multiply(multiply((vec2){0.0f,6.28318531f},pOutExponentialMoment[0]),pTrigonometricMoment[1]);
	pOutExponentialMoment[2]=add(multiply(multiply((vec2){0.0f,6.28318531f},pOutExponentialMoment[0]),pTrigonometricMoment[2]),multiply(multiply((vec2){0.0f,3.14159265f},pOutExponentialMoment[1]),pTrigonometricMoment[1]));
	pOutExponentialMoment[0]=multiply_mixed(2.0f,pOutExponentialMoment[0]);
}

void computeAutocorrelation3(vec2 pOutAutocorrelation[3],const vec2 pSignal[3]){
	pOutAutocorrelation[0]=add(add(multiply(pSignal[0],conjugate(pSignal[0])),multiply(pSignal[1],conjugate(pSignal[1]))),multiply(pSignal[2],conjugate(pSignal[2])));
	pOutAutocorrelation[1]=add(multiply(pSignal[0],conjugate(pSignal[1])),multiply(pSignal[1],conjugate(pSignal[2])));
	pOutAutocorrelation[2]=multiply(pSignal[0],conjugate(pSignal[2]));
}

void computeImaginaryCorrelation3(float pOutCorrelation[3],const vec2 pLHS[3],const vec2 pRHS[3]){
	pOutCorrelation[0]=pLHS[0].x*pRHS[0].y+pLHS[0].y*pRHS[0].x+pLHS[1].x*pRHS[1].y+pLHS[1].y*pRHS[1].x+pLHS[2].x*pRHS[2].y+pLHS[2].y*pRHS[2].x;
	pOutCorrelation[1]=pLHS[1].x*pRHS[0].y+pLHS[1].y*pRHS[0].x+pLHS[2].x*pRHS[1].y+pLHS[2].y*pRHS[1].x;
	pOutCorrelation[2]=pLHS[2].x*pRHS[0].y+pLHS[2].y*pRHS[0].x;
}



// call these
void prepareReflectanceSpectrumLagrangeReal3(float pOutLagrangeMultiplier[3],const float pTrigonometricMoment[3]){
	vec2 pExponentialMoment[3];
	trigonometricToExponentialMomentsReal3(pExponentialMoment,pTrigonometricMoment);
	vec2 pEvaluationPolynomial[3];
	levinsonsAlgorithm3(pEvaluationPolynomial,pExponentialMoment);
	pEvaluationPolynomial[0]=multiply_mixed(6.28318531f,pEvaluationPolynomial[0]);
	pEvaluationPolynomial[1]=multiply_mixed(6.28318531f,pEvaluationPolynomial[1]);
	pEvaluationPolynomial[2]=multiply_mixed(6.28318531f,pEvaluationPolynomial[2]);
	vec2 pAutocorrelation[3];
	computeAutocorrelation3(pAutocorrelation,pEvaluationPolynomial);
	pExponentialMoment[0]=multiply_mixed(0.5f,pExponentialMoment[0]);
	computeImaginaryCorrelation3(pOutLagrangeMultiplier,pAutocorrelation,pExponentialMoment);
	float normalizationFactor=1.0f/(3.14159265f*pEvaluationPolynomial[0].x);
	pOutLagrangeMultiplier[0]=normalizationFactor*pOutLagrangeMultiplier[0];
	pOutLagrangeMultiplier[1]=normalizationFactor*pOutLagrangeMultiplier[1];
	pOutLagrangeMultiplier[2]=normalizationFactor*pOutLagrangeMultiplier[2];
}

float evaluateReflectanceSpectrumRealLagrange3(const float phase,const float pLagrangeMultiplier[3]){
	vec2 conjCirclePoint;
	conjCirclePoint=vec2(cos(-phase),sin(-phase));
	float lagrangeSeries;
	lagrangeSeries=evaluateRealFourierSeries3(conjCirclePoint,pLagrangeMultiplier);
	return fast_atan(lagrangeSeries)*0.318309886+0.500000000;
}
#endif
