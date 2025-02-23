// eigenvalue decomposition for symmetric 2x2 and 3x3 matrices.
// (c) christoph peters, originally used for florian reibold's path guiding code.

void getEVDSymmetric2x2(out vec2 pOutEigenvalues, out mat2 pOutEigenvectors, in mat2 pMatrix)
{
	// Define some short hands for the matrix entries
	float a = pMatrix[0][0], b = pMatrix[0][1], c = pMatrix[1][1];
	// Compute coefficients of the characteristic polynomial and solve
	float pHalf = -0.5 * (a + c), q = a*c - b*b;
	float discriminant_root = sqrt(pHalf * pHalf - q);
	pOutEigenvalues = -pHalf + vec2(discriminant_root, - discriminant_root);
	// Subtract a scaled identity matrix to obtain a rank one matrix
	float a0 = a - pOutEigenvalues.x, b0 = b, c0 = c - pOutEigenvalues.x;
	// The column space of this matrix is orthogonal to the first eigenvector 
	// and since the eigenvectors are orthogonal themselves, it agrees with the 
	// second eigenvector. Pick the longer column to avoid cancellation.
	float slen0 = a0*a0 + b0*b0;
	float slen1 = b0*b0 + c0*c0;
	if (slen0 > slen1) pOutEigenvectors[1] = vec2(a0, b0);
	else               pOutEigenvectors[1] = vec2(b0, c0);
	// If the eigenvector is exactly zero, both eigenvalues are the same and the 
	// choice of orthogonal eigenvectors is arbitrary
  if(max(slen0, slen1) == 0.0)
  {
    pOutEigenvectors = mat2(1, 0, 0, 1);
  }
  else
  { // normalise and rotate to get the other eigenvector
    pOutEigenvectors[1] = normalize(pOutEigenvectors[1]);
    pOutEigenvectors[0] = vec2(pOutEigenvectors[1].y, - pOutEigenvectors[1].x);
  }
}

// this utility function swaps the two floats if they 
// are not sorted in descending order.
void sortDescending2(inout vec2 v)
{
  if(v.x < v.y) v = v.yx;
}

// sorts an array of three entries in descending order (in place).
void sortDescending3(inout vec3 v)
{ // bubble sort
	sortDescending2(v.xz);
	sortDescending2(v.xy);
	sortDescending2(v.yz);
}

// Given coefficients of a normalized, cubic polynomial
// x^3 + pCoefficient[2]*x^2  + pCoefficient[1]*x  + pCoefficient[0]
// that is known to have three real roots, this function computes and returns 
// all roots sorted in descending order. Behavior is undefined if there are 
// not three real roots.
void solveCubic(out vec3 pOutRoots, in vec3 pCoefficient)
{
	// Divide middle coefficients by three
  vec3 xyz = vec3(pCoefficient.x, pCoefficient.y/3.0, pCoefficient.z/3.0);
	// Compute the Hessian and the discrimant
	vec3 pDelta = vec3(-xyz.z*xyz.z + xyz.y,-xyz.y*xyz.z + xyz.x,xyz.z*xyz.x - xyz.y*xyz.y);
	float discriminant = 4.0 * pDelta.x * pDelta.z - pDelta.y * pDelta.y;
	float discriminantRoot = sqrt((discriminant < 0.0) ? 0.0 : discriminant);
	// Compute the root of largest magnitude
	float largestRoot;
	{ // Compute coefficients of the depressed cubic (third is zero, fourth 
		// is one)
		float p = pDelta.x;
		float q = -2.0 * xyz.z * pDelta.x + pDelta.y;
		// Take the cubic root of a normalized complex number (or maybe its 
		// conjugate, we only care about the real part)
		float theta = abs(atan(discriminantRoot, -q) / 3.0);
		float cubicRootX = cos(theta);
		float cubicRootY = sin(theta);
		// Take the real part of two of the three cubic roots. That literal is 
		// sqrt(3)/2.
		float rootCandidate0 = cubicRootX;
		float rootCandidate1 = -0.5 * cubicRootX - 0.86602540378443864676372317075294 * cubicRootY;
		// Apply the scaling of the depression transform
		float sqrtP = sqrt((p <= 0.0) ? (-p) : 0.0);
		rootCandidate0 *= 2.0 * sqrtP;
		rootCandidate1 *= 2.0 * sqrtP;
		// Pick the appropriate result
		largestRoot = ((rootCandidate0 + rootCandidate1) > 2.0 * xyz.z) ? rootCandidate0 : rootCandidate1;
		// Apply the shift of the depression transform
		largestRoot -= xyz.z;
	}
	// Compute the root of least magnitude
	float smallestRoot;
	{ // Compute coefficients of the depressed cubic when working with a 
		// flipped polynomial where x is replaced by 1/x (third is zero)
		float A = xyz.x;
		float p = pDelta.z;
		float q = -A * pDelta.y + 2.0 * xyz.y * pDelta.z;
		// Take the cubic root of a normalized complex number (or maybe its 
		// conjugate, we only care about the real part)
		float theta = abs(atan(A * discriminantRoot, -q) / 3.0);
		float cubicRootX = cos(theta);
		float cubicRootY = sin(theta);
		// Take the real part of two of the three cubic roots. That literal is sqrt(3)/2.
		float rootCandidate0 = cubicRootX;
		float rootCandidate1 = -0.5 * cubicRootX - 0.86602540378443864676372317075294 * cubicRootY;
		// Apply the scaling of the depression transform
		float sqrtP = sqrt((p <= 0.0) ? (-p) : 0.0);
		rootCandidate0 *= 2.0*sqrtP;
		rootCandidate1 *= 2.0*sqrtP;
		// Pick the appropriate result
		smallestRoot = (rootCandidate0 + rootCandidate1 < 2.0 * xyz.y) ? rootCandidate0 : rootCandidate1;
		// Apply the shift of the depression transform, account for the leading 
		// coefficient and undo the reciprocal transform
		smallestRoot = -A / (smallestRoot + xyz.y);
	}
	// Compute the root of intermediate magnitude through polynomial division
	float F = -largestRoot - smallestRoot;
	float G =  largestRoot * smallestRoot;
	float intermediateRoot = (xyz.y*F - xyz.z*G) / (-xyz.z*F + xyz.y);
	// Output and sort the roots (thus far they are only sorted by magnitude)
	pOutRoots = vec3(largestRoot, intermediateRoot, smallestRoot);
	sortDescending3(pOutRoots);
}

// Given a 3x3 matrix (or three 3D vectors, one after the other) this function 
// outputs a normalized version the column of greatest norm (or the longest 
// vector). If the matrix is zero entirely, it outputs an arbitrary normalized 
// vector.
vec3 selectLongestColumnAndNormalize(in mat3 M)
{
	float maxSquaredLength = -1.0;
	int iLongest = 0;
	for (int i = 0; i != 3; ++i)
  {
    float squaredLength = dot(M[i],M[i]);
		iLongest = (maxSquaredLength < squaredLength) ? i : iLongest;
		maxSquaredLength = (maxSquaredLength < squaredLength) ? squaredLength : maxSquaredLength;
	}
	if (maxSquaredLength == 0.0)
    return vec3(1.0, 0.0, 0.0);

  float invLength = 1.0 / sqrt(maxSquaredLength);
  return invLength * M[iLongest];
}

// checks whether the given two floating point values are nearly equal
bool equalWithTolerance(float lhs, float rhs)
{
	const float epsilon = 1e-5;
	bool equalSign = ((lhs * rhs) >= 0.0);
	lhs = abs(lhs);
	rhs = abs(rhs);
	return equalSign && lhs <= (1.0 + epsilon) * rhs && lhs >= (1.0 - epsilon) * rhs;
}

void getEVDSymmetric3x3(
    out vec3 pOutEigenvalues,
    out mat3 pOutEigenvectors, // column vectors are eigenvectors
    in  mat3 M)
{
	// Compute coefficients of the characteristic polynomial.
  vec3 pCoefficient = vec3(
      - determinant(M), // constant coef: minus determinant
      // linear coef: some weird cofactors:
      (M[0][0]*M[1][1] + M[1][1]*M[2][2] + M[0][0]*M[2][2]) - (M[2][1]*M[2][1] + M[2][0]*M[2][0] + M[1][0]*M[1][0]),
      - M[0][0] - M[1][1] - M[2][2]); // quadratic coef: minus the trace
	// Compute the eigenvalues
	solveCubic(pOutEigenvalues, pCoefficient);

	// The case of two nearly identical eigenvalues needs to be handled 
	// separately for robustness reasons
	int iUniqueEigenvalue = -1; // can't be == 1 since the evs come in sorted
	int iSecondEigenvalue, iThirdEigenvalue;
  float ev = 0.0;
	if(equalWithTolerance(pOutEigenvalues[0], pOutEigenvalues[1]))
  { // Make the two eigenvalues truly equal
		iUniqueEigenvalue = 2;
    ev = 0.5 * (pOutEigenvalues[0] + pOutEigenvalues[1]);
    pOutEigenvalues[0] = pOutEigenvalues[1] = ev;
	}
	else if(equalWithTolerance(pOutEigenvalues[1], pOutEigenvalues[2]))
  {
		iUniqueEigenvalue = 0;
    ev = 0.5 * (pOutEigenvalues[1] + pOutEigenvalues[2]);
    pOutEigenvalues[1] = pOutEigenvalues[2] = ev;
	}
	if(iUniqueEigenvalue >= 0)
  { // The eigenvector for the unique eigenvalue is in the one-dimensional 
		// column space of the matrix minus the other eigenvalue
    mat3 pSubtractedMatrix = mat3(
        M[0] - vec3(ev, 0, 0),
        M[1] - vec3(0, ev, 0),
        M[2] - vec3(0, 0, ev));
		vec3 pUniqueEigenvector = selectLongestColumnAndNormalize(pSubtractedMatrix);
		// The other two vectors are orthonormal but arbitrary
		mat3 pOrthogonal;
		pOrthogonal[0] = cross(pUniqueEigenvector, vec3(1.0, 0.0, 0.0));
		pOrthogonal[1] = cross(pUniqueEigenvector, vec3(0.0, 1.0, 0.0));
		pOrthogonal[2] = cross(pUniqueEigenvector, vec3(0.0, 0.0, 1.0));
		vec3 pSecondEigenvector = selectLongestColumnAndNormalize(pOrthogonal);
		vec3 pThirdEigenvector = cross(pUniqueEigenvector, pSecondEigenvector);

    if(iUniqueEigenvalue == 0)
    { // store the eigenvectors
      pOutEigenvectors[0] = pUniqueEigenvector;
      pOutEigenvectors[1] = pSecondEigenvector;
      pOutEigenvectors[2] = pThirdEigenvector;
    }
    else
    { // unique gotta be 2
      pOutEigenvectors[2] = pUniqueEigenvector;
      pOutEigenvectors[0] = pSecondEigenvector;
      pOutEigenvectors[1] = pThirdEigenvector;
    }
		return;
	}
  // iUniqueEigenvalue = -1, regular case
	// Otherwise, we compute the first two eigenvectors independently
  // Subtract the identity matrix times the eigenvalue
  { // first ev
    mat3 b = mat3(
        M[0] - vec3(pOutEigenvalues[0], 0, 0),
        M[1] - vec3(0, pOutEigenvalues[0], 0),
        M[2] - vec3(0, 0, pOutEigenvalues[0]));
    // The eigenvector is orthogonal to the column space of this matrix. 
    // Find it by taking the cross product of two columns. For better 
    // stabiliy, we try all three pairs.
    mat3 pCandidate = mat3(
        cross(b[0], b[1]),
        cross(b[1], b[2]),
        cross(b[2], b[0]));
    pOutEigenvectors[0] = selectLongestColumnAndNormalize(pCandidate);
  }{ // second ev
    mat3 b = mat3(
        M[0] - vec3(pOutEigenvalues[1], 0, 0),
        M[1] - vec3(0, pOutEigenvalues[1], 0),
        M[2] - vec3(0, 0, pOutEigenvalues[1]));
    mat3 pCandidate = mat3(
        cross(b[0], b[1]),
        cross(b[1], b[2]),
        cross(b[2], b[0]));
    pOutEigenvectors[1] = selectLongestColumnAndNormalize(pCandidate);
  }{ // Explicitly enforce orthogonality
    float d = dot(pOutEigenvectors[0], pOutEigenvectors[1]);
    pOutEigenvectors[1] -= d * pOutEigenvectors[0];
    pOutEigenvectors[1] = normalize(pOutEigenvectors[1]);
	}
	// The last one is orthogonal to the first two
  pOutEigenvectors[2] = cross(pOutEigenvectors[0], pOutEigenvectors[1]);
}
