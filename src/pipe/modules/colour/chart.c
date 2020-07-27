// this is the equivalent of darktable-chart,
// an external tool to match thin plate spline RBF to a full set of
// many colour checkers (say IT8 with 288 patches), using only a minimum
// number of control points. it uses some variant of orthogonal matching
// pursuit to do the search for a good minimal configuration.



static inline double compute_error(
    const tonecurve_t *c,
    const double **target,
    const double *residual_L,
    const double *residual_a,
    const double *residual_b,
    const int wd,
    double *maxerr)
{
  // compute error:
  double err = 0.0;
  double merr = 0.0;
  for(int i = 0; i < wd; i++)
  {
    const double localerr = sqrt(residual_L[i] * residual_L[i] + residual_a[i] * residual_a[i] + residual_b[i] * residual_b[i]);
    err += localerr/wd;
    merr = MAX(merr, localerr);
  }
  if(maxerr) *maxerr = merr;
  return err;
}

static inline int solve(double *As, double *w, double *v, const double *b, double *coeff, int wd, int s, int S)
{
  // A'[wd][s+1] = u[wd][s+1] diag(w[s+1]) v[s+1][s+1]^t
  //
  // svd to solve for c:
  // A * c = b
  // A = u w v^t => A-1 = v 1/w u^t
  dsvd(As, wd, s + 1, S, w, v); // As is wd x s+1 but row stride S.
  if(w[s] < 1e-3)               // if the smallest singular value becomes too small, we're done
    return 1;
  double *tmp = malloc(S * sizeof(double));
  for(int i = 0; i <= s; i++) // compute tmp = u^t * b
  {
    tmp[i] = 0.0;
    for(int j = 0; j < wd; j++) tmp[i] += As[j * S + i] * b[j];
  }
  for(int i = 0; i <= s; i++) // apply singular values:
    tmp[i] /= w[i];
  for(int j = 0; j <= s; j++)
  { // compute first s output coefficients coeff[j]
    coeff[j] = 0.0;
    for(int i = 0; i <= s; i++) coeff[j] += v[j * (s + 1) + i] * tmp[i];
  }
  free(tmp);
  return 0;
}

// returns sparsity <= S
int thinplate_match(
    const tonecurve_t *curve, // tonecurve to apply after this (needed for error estimation)
    int dim,                  // dimensionality of points
    int N,                    // number of points
    const double *point,      // dim-strided points
    const double **target,    // target values, one pointer per dimension
    int S,                    // desired sparsity level, actual result will be returned
    int *permutation, // pointing to original order of points, to identify correct output coeff
    double **coeff,   // output coefficient arrays for each dimension, ordered according to
    // permutation[dim]
    double *avgerr,           // average error
    double *maxerr)           // max error
{
  if(avgerr) *avgerr = 0.0;
  if(maxerr) *maxerr = 0.0;

  const int wd = N + 4;
  double *A = malloc(wd * wd * sizeof(double));
  // construct system matrix A such that:
  // A c = f
  //
  // | R   P | |c| = |f|
  // | P^t 0 | |d|   |0|
  //
  // to interpolate values f_i with the radial basis function system matrix R and a polynomial term P.
  // P is a 3D linear polynomial a + b x + c y + d z
  //
  // radial basis function part R
  for(int j = 0; j < N; j++)
    for(int i = j; i < N; i++) A[j * wd + i] = A[i * wd + j] = thinplate_kernel(point + 3 * i, point + 3 * j);

  // polynomial part P: constant + 3x linear
  for(int i = 0; i < N; i++) A[i * wd + N + 0] = A[(N + 0) * wd + i] = 1.0f;
  for(int i = 0; i < N; i++) A[i * wd + N + 1] = A[(N + 1) * wd + i] = point[3 * i + 0];
  for(int i = 0; i < N; i++) A[i * wd + N + 2] = A[(N + 2) * wd + i] = point[3 * i + 1];
  for(int i = 0; i < N; i++) A[i * wd + N + 3] = A[(N + 3) * wd + i] = point[3 * i + 2];

  for(int j = N; j < wd; j++)
    for(int i = N; i < wd; i++) A[j * wd + i] = 0.0f;

  // precompute normalisation factors for columns of A
  double *norm = malloc(wd * sizeof(double));
  for(int i = 0; i < wd; i++)
  {
    norm[i] = 0.0;
    for(int j = 0; j < wd; j++) norm[i] += A[j * wd + i] * A[j * wd + i];
    norm[i] = 1.0 / sqrt(norm[i]);
  }

  // XXX do we need these explicitly?
  // residual = target vector
  double(*r)[wd] = malloc(dim * wd * sizeof(double));
  const double **b = malloc(dim * sizeof(double *));
  for(int k = 0; k < dim; k++) b[k] = target[k];
  for(int k = 0; k < dim; k++) memcpy(r[k], b[k], wd * sizeof(double));

  double *w = malloc(S * sizeof(double));
  double *v = malloc(S * S * sizeof(double));
  double *As = calloc(wd * S, sizeof(double));

  // for rank from 0 to sparsity level
  int s = 0, patches = 0;
  double olderr = FLT_MAX;
  // in case of replacement, iterate all the way to wd
  for(; s < wd; s++)
  {
    const int sparsity = MIN(s, S);
    if(patches >= S - 4)
    {
      free(r);
      free(b);
      free(w);
      free(v);
      free(As);
      free(norm);
      free(A);
      return sparsity;
    }
    assert(sparsity < S + 4);
    // find (sparsity+1)-th column a_m by m = argmax_t{ a_t^t r . norm_t}
    // by searching over all three residuals
    double maxdot = 0.0;
    int maxcol = 0;
    for(int t = 0; t < wd; t++)
    {
      double dot = 0.0;
      if(norm[t] > 0.0)
      {
        for(int ch = 0; ch < dim; ch++)
        {
          double chdot = 0.0;
          for(int j = 0; j < wd; j++) chdot += A[j * wd + t] * r[ch][j];
          dot += fabs(chdot);
        }
        dot *= norm[t];
      }
      // fprintf(stderr, "dot %d = %g\n", i, dot);
      if(dot > maxdot)
      {
        maxcol = t;
        maxdot = dot;
      }
    }

    if(patches < S - 4)
    {
      // remember which column that was, we'll need it to evaluate later:
      permutation[s] = maxcol;
      if(maxcol < N) patches++;
      // make sure we won't choose it again:
      norm[maxcol] = 0.0;
    }
    else
    { // already have chosen S-4 patches as columns, now do the replacement:
      int mincol = 0;
      double minerr = FLT_MAX;
      for(int t = 0; t < sparsity; t++)
      {
        // find already chosen column t with min error reduction when replacing
        // XXX do we set perm[t] above, ever? for t=S-1?
        int oldperm = permutation[t];
        permutation[t] = maxcol;
        double dot = 0.0;
        for(int ch = 0; ch < dim; ch++)
        {
          double chdot = 0.0;
          for(int j = 0; j < wd; j++) chdot += A[j * wd + t] * r[ch][j];
          dot += fabs(chdot);
        }
        double n = 0.0; // recompute column norm
        for(int j = 0; j < wd; j++) n += A[j * wd + mincol] * A[j * wd + mincol];
        dot *= n;
        double err = 1. / dot;

        if(err < minerr)
        {
          mincol = t;
          minerr = err;
        }
        permutation[t] = oldperm;
        // fprintf(stderr, "perm %d %d\n", t, oldperm);
      }
      // if(minerr >= 1. / maxdot) return sparsity + 1;
      if(minerr < 1. / maxdot)
      {
        fprintf(stderr, "replacing %d <- %d\n", mincol, maxcol);
        // replace column
        permutation[mincol] = maxcol;
        // reset norm[] of discarded column to something > 0
        norm[mincol] = 0.0;
        for(int j = 0; j < wd; j++) norm[mincol] += A[j * wd + mincol] * A[j * wd + mincol];
        norm[mincol] = 1.0 / sqrt(norm[mincol]);
        norm[maxcol] = 0.0;
      }
    }

    const int sp = MIN(sparsity, S-1); // need to fix up for replacement
    // solve linear least squares for sparse c for every output channel:
    for(int ch = 0; ch < dim; ch++)
    {
      // re-init all of the previous columns in As since
      // svd will destroy its contents:
      for(int i = 0; i <= sp; i++)
        for(int j = 0; j < wd; j++) As[j * S + i] = A[j * wd + permutation[i]];

      // on error, return last valid configuration
      if(solve(As, w, v, b[ch], coeff[ch], wd, sp, S))
      {
        free(r);
        free(b);
        free(w);
        free(v);
        free(As);
        free(norm);
        free(A);
        return sparsity;
      }

      // compute new residual:
      // r = b - As c
      for(int j = 0; j < wd; j++)
      {
        r[ch][j] = b[ch][j];
        for(int i = 0; i <= sp; i++) r[ch][j] -= A[j * wd + permutation[i]] * coeff[ch][i];
      }
    }

    double merr = 0.0;
    const double err = compute_error(curve, target, r[0], r[1], r[2], wd, &merr);
    // residual is max CIE76 delta E now
    // everything < 2 is usually considired a very good approximation:
    if(patches == S-4)
    {
      if(avgerr) *avgerr = err;
      if(maxerr) *maxerr = merr;
      fprintf(stderr, "rank %d/%d avg DE %g max DE %g\n", sp + 1, patches, err, merr);
    }
    if(s >= S && err >= olderr)
      fprintf(stderr, "error increased!\n");
      // return sparsity + 1;
       // if(err < 2.0) return sparsity+1;
    olderr = err;
  }
  free(r);
  free(b);
  free(w);
  free(v);
  free(As);
  free(norm);
  free(A);
  return -1;
}
