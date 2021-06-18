#pragma once
#include <math.h>
#include <assert.h>
#include "complex_algebra.h"


void trigonometricToExponentialMomentsReal3(float_complex pOutExponentialMoment[3],const float pTrigonometricMoment[3]){
	float zerothMomentPhase=3.14159265f*pTrigonometricMoment[0]-1.57079633f;
	pOutExponentialMoment[0]=(float_complex){cosf(zerothMomentPhase),sinf(zerothMomentPhase)};
	pOutExponentialMoment[0]=multiply_mixed(0.0795774715f,pOutExponentialMoment[0]);
	pOutExponentialMoment[1]=multiply_mixed(pTrigonometricMoment[1],multiply((float_complex){0.0f,6.28318531f},pOutExponentialMoment[0]));
	pOutExponentialMoment[2]=add(multiply_mixed(pTrigonometricMoment[2],multiply((float_complex){0.0f,6.28318531f},pOutExponentialMoment[0])),multiply_mixed(pTrigonometricMoment[1],multiply((float_complex){0.0f,3.14159265f},pOutExponentialMoment[1])));
	pOutExponentialMoment[0]=multiply_mixed(2.0f,pOutExponentialMoment[0]);
}

void levinsonsAlgorithm3(float_complex pOutSolution[3], float_complex pFirstColumn[3])
{
  // float onemeps = 0.9999f;
  // float corrf = 1.0f/(1.0f-onemeps*onemeps);
	pOutSolution[0]=(float_complex){1.0f/(pFirstColumn[0].x),0.0f};
	float_complex dotProduct;
  float dotProductAbsSquare;
	float_complex flippedSolution1;
	float_complex flippedSolution2;
	float factor;
	dotProduct=multiply_mixed(pOutSolution[0].x,pFirstColumn[1]);
  // biasing
  dotProductAbsSquare = absSqr(dotProduct);
	factor=1.0f/(1.0f-dotProductAbsSquare);
#if 0
  if(factor < 0.0f)
  {
    dotProduct = multiply_mixed(onemeps*1.0f/sqrtf(dotProductAbsSquare), dotProduct);
    pFirstColumn[1] = multiply_mixed(1.0f/pOutSolution[0].x, dotProduct);
    factor = corrf;
    // onemeps = 0.0f;
    // corrf = 1.0f;
  }
#endif
	flippedSolution1=(float_complex){pOutSolution[0].x,0.0f};
	pOutSolution[0]=(float_complex){factor*pOutSolution[0].x,0.0f};
	pOutSolution[1]=multiply_mixed(factor,(negate(multiply_mixed(flippedSolution1.x, dotProduct))));
  float_complex unnormalised_next_moment_center = multiply(pOutSolution[1], pFirstColumn[1]);
	dotProduct = add(multiply_mixed(pOutSolution[0].x,pFirstColumn[2]), unnormalised_next_moment_center);
  // biasing
  dotProductAbsSquare = absSqr(dotProduct);
	factor=1.0f/(1.0f-dotProductAbsSquare);
#if 0
  if(factor < 0.0f)
  {
    dotProduct = multiply_mixed(onemeps*1.0f/sqrtf(dotProductAbsSquare), dotProduct);
    pFirstColumn[2] = multiply_mixed(1.0f/pOutSolution[0].x, subtract(dotProduct, unnormalised_next_moment_center));
    factor = corrf;
  }
#endif
	flippedSolution1=conjugate(pOutSolution[1]);
	flippedSolution2=(float_complex){pOutSolution[0].x,0.0f};
	pOutSolution[0]=(float_complex){factor*pOutSolution[0].x,0.0f};
	pOutSolution[1]=multiply_mixed(factor,(subtract(pOutSolution[1],multiply(flippedSolution1,dotProduct))));
	pOutSolution[2]=multiply_mixed(factor,(negate(multiply_mixed(flippedSolution2.x,dotProduct))));
}

float_complex evaluateFastHerglotzTransform3(const float_complex circlePoint,const float_complex pExponentialMoment[3],const float_complex pEvaluationPolynomial[3]){
	float_complex conjCirclePoint=conjugate(circlePoint);
	float polynomial2=pEvaluationPolynomial[0].x;
	float_complex polynomial1=add(pEvaluationPolynomial[1],multiply_mixed(polynomial2,conjCirclePoint));
	float_complex polynomial0=add(pEvaluationPolynomial[2],multiply(conjCirclePoint,polynomial1));
	float_complex dotProduct=add(multiply(polynomial1,pExponentialMoment[1]),multiply_mixed(polynomial2,pExponentialMoment[2]));
	return add(pExponentialMoment[0],multiply_mixed(2.0f,divide(dotProduct,polynomial0)));
}

void prepareReflectanceSpectrumReal3(float_complex pOutExponentialMoment[3],float_complex pOutEvaluationPolynomial[3],const float pTrigonometricMoment[3]){
	trigonometricToExponentialMomentsReal3(pOutExponentialMoment,pTrigonometricMoment);
	levinsonsAlgorithm3(pOutEvaluationPolynomial,pOutExponentialMoment);
	pOutEvaluationPolynomial[0]=multiply_mixed(6.28318531f,pOutEvaluationPolynomial[0]);
	pOutEvaluationPolynomial[1]=multiply_mixed(6.28318531f,pOutEvaluationPolynomial[1]);
	pOutEvaluationPolynomial[2]=multiply_mixed(6.28318531f,pOutEvaluationPolynomial[2]);
}

float evaluateReflectanceSpectrum3(const float phase,const float_complex pExponentialMoment[3],const float_complex pEvaluationPolynomial[3]){
	float_complex circlePoint;
  // could compute this at reduced precision, our wavelength is ~5nm anyways
	circlePoint=(float_complex){cosf(phase),sinf(phase)};
	float_complex herglotzTransform;
	herglotzTransform=evaluateFastHerglotzTransform3(circlePoint,pExponentialMoment,pEvaluationPolynomial);
	return fast_atan(herglotzTransform.y/herglotzTransform.x)*0.318309886f+0.500000000f;
}



/// lagrangian stuff here:

// helpers

float evaluateRealFourierSeries3(const float_complex circlePoint,const float pFourierCoefficient[3]){
	// Minimize sequential dependencies by computing powers of circlePoint
	// by multiplying previous powers by powers of two
	float_complex circlePointPower1=circlePoint;
	float_complex circlePointPower2=multiply(circlePointPower1,circlePointPower1);
	float result=pFourierCoefficient[1]*circlePointPower1.x+pFourierCoefficient[2]*circlePointPower2.x;
	return 2.0f*result+pFourierCoefficient[0];
}

void trigonometricToExponentialMoments3(float_complex pOutExponentialMoment[3],const float_complex pTrigonometricMoment[3]){
	float zerothMomentPhase=3.14159265f*pTrigonometricMoment[0].x-1.57079633f;
	pOutExponentialMoment[0]=(float_complex){cosf(zerothMomentPhase),sinf(zerothMomentPhase)};
	pOutExponentialMoment[0]=multiply_mixed(0.0795774715f,pOutExponentialMoment[0]);
	pOutExponentialMoment[1]=multiply(multiply((float_complex){0.0f,6.28318531f},pOutExponentialMoment[0]),pTrigonometricMoment[1]);
	pOutExponentialMoment[2]=add(multiply(multiply((float_complex){0.0f,6.28318531f},pOutExponentialMoment[0]),pTrigonometricMoment[2]),multiply(multiply((float_complex){0.0f,3.14159265f},pOutExponentialMoment[1]),pTrigonometricMoment[1]));
	pOutExponentialMoment[0]=multiply_mixed(2.0f,pOutExponentialMoment[0]);
}

void computeAutocorrelation3(float_complex pOutAutocorrelation[3],const float_complex pSignal[3]){
	pOutAutocorrelation[0]=add(add(multiply(pSignal[0],conjugate(pSignal[0])),multiply(pSignal[1],conjugate(pSignal[1]))),multiply(pSignal[2],conjugate(pSignal[2])));
	pOutAutocorrelation[1]=add(multiply(pSignal[0],conjugate(pSignal[1])),multiply(pSignal[1],conjugate(pSignal[2])));
	pOutAutocorrelation[2]=multiply(pSignal[0],conjugate(pSignal[2]));
}

void computeImaginaryCorrelation3(float pOutCorrelation[3],const float_complex pLHS[3],const float_complex pRHS[3]){
	pOutCorrelation[0]=pLHS[0].x*pRHS[0].y+pLHS[0].y*pRHS[0].x+pLHS[1].x*pRHS[1].y+pLHS[1].y*pRHS[1].x+pLHS[2].x*pRHS[2].y+pLHS[2].y*pRHS[2].x;
	pOutCorrelation[1]=pLHS[1].x*pRHS[0].y+pLHS[1].y*pRHS[0].x+pLHS[2].x*pRHS[1].y+pLHS[2].y*pRHS[1].x;
	pOutCorrelation[2]=pLHS[2].x*pRHS[0].y+pLHS[2].y*pRHS[0].x;
}



// call these
void prepareReflectanceSpectrumLagrangeReal3(float pOutLagrangeMultiplier[3],const float pTrigonometricMoment[3]){
	float_complex pExponentialMoment[3];
	trigonometricToExponentialMomentsReal3(pExponentialMoment,pTrigonometricMoment);
	float_complex pEvaluationPolynomial[3];
	levinsonsAlgorithm3(pEvaluationPolynomial,pExponentialMoment);
	pEvaluationPolynomial[0]=multiply_mixed(6.28318531f,pEvaluationPolynomial[0]);
	pEvaluationPolynomial[1]=multiply_mixed(6.28318531f,pEvaluationPolynomial[1]);
	pEvaluationPolynomial[2]=multiply_mixed(6.28318531f,pEvaluationPolynomial[2]);
	float_complex pAutocorrelation[3];
	computeAutocorrelation3(pAutocorrelation,pEvaluationPolynomial);
	pExponentialMoment[0]=multiply_mixed(0.5f,pExponentialMoment[0]);
	computeImaginaryCorrelation3(pOutLagrangeMultiplier,pAutocorrelation,pExponentialMoment);
	float normalizationFactor=1.0f/(3.14159265f*pEvaluationPolynomial[0].x);
	pOutLagrangeMultiplier[0]=normalizationFactor*pOutLagrangeMultiplier[0];
	pOutLagrangeMultiplier[1]=normalizationFactor*pOutLagrangeMultiplier[1];
	pOutLagrangeMultiplier[2]=normalizationFactor*pOutLagrangeMultiplier[2];
}

float evaluateReflectanceSpectrumRealLagrange3(const float phase,const float pLagrangeMultiplier[3]){
	float_complex conjCirclePoint;
	conjCirclePoint=(float_complex){cosf(-phase),sinf(-phase)};
	float lagrangeSeries;
	lagrangeSeries=evaluateRealFourierSeries3(conjCirclePoint,pLagrangeMultiplier);
	return fast_atan(lagrangeSeries)*0.318309886f+0.500000000f;
}

