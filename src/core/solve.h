#pragma once

#include <stdint.h>
#include <alloca.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <float.h>

// conjugate gradient solve:
static inline double
dt_conj_grad(const double *A, const double *b, double *x, const int m)
{
  for(int j=0;j<m;j++) x[j] = 0.0;
  double *r = alloca(sizeof(double)*m);
  for(int j=0;j<m;j++) r[j] = b[j];

  double *p = alloca(sizeof(double)*m);
  for(int j=0;j<m;j++) p[j] = r[j];

  double rsold = 0.0;
  for(int j=0;j<m;j++) rsold += r[j]*r[j];

  double *Ap = alloca(sizeof(double)*m);
  for(int i=0;i<m;i++)
  {
    memset(Ap, 0, sizeof(double)*m);
    for(int j=0;j<m;j++)
      for(int k=0;k<m;k++)
        Ap[j] += A[m*j+k] * p[k];
    
    double pAp = 0.0;
    for(int j=0;j<m;j++)
      pAp += p[j] * Ap[j];
    double alpha = rsold / pAp;
    // assert(alpha == alpha);
    if(!(alpha == alpha)) return 0.1;//0.0;
    for(int j=0;j<m;j++)
      x[j] += alpha * p[j];
    for(int j=0;j<m;j++)
      r[j] -= alpha * Ap[j];
    double rsnew = 0.0;
    for(int j=0;j<m;j++) rsnew += r[j]*r[j];
    if(sqrt(rsnew) < 2e-6) return sqrt(rsnew);
    if(rsnew > rsold) return sqrt(rsnew);
    for(int j=0;j<m;j++) p[j] = r[j] + rsnew / rsold * p[j];
    rsold = rsnew;
  }
  return sqrt(rsold);
}

static inline double
dt_gauss_newton_cg_step(
    const int     m,      // number of parameters m < n
    double       *p,      // n parameters, will be updated
    const int     n,      // number of function and target values
    const double *f,      // n function values m < n
    const double *J,      // jacobian m x n (df0/dp0, df0/dp1, .. df1/dp0, ...dfn-1/dpm-1)
    const double *t)      // n target values
{
  // compute b vector Jt r in R[m], residual r in R[n]
  double *b = alloca(sizeof(double)*m);
  memset(b, 0, sizeof(double)*m);
  for(int i=0;i<m;i++)
    for(int j=0;j<n;j++)
      b[i] += J[m*j + i] * (t[j] - f[j]);

  // compute dense square matrix Jt J
  double *A = alloca(sizeof(double)*m*m);
  double **Ar = alloca(sizeof(double*)*m);
  for(int i=0;i<m;i++) Ar[i] = A+i*m;
  memset(A, 0, sizeof(double)*m*m);
  // TODO: exploit symmetry
  // TODO: don't compute but pass to CG
  for(int j=0;j<m;j++)
    for(int i=0;i<m;i++)
      for(int k=0;k<n;k++)
        A[j*m+i] += J[k*m+i] * J[k*m+j];

  // now solve
  //        Delta = (Jt J)^-1 Jt r
  // (Jt J) Delta = Jt r
  //   A    x     = b

  double *delta = alloca(sizeof(double)*m);
  double resid = dt_conj_grad(A, b, delta, m);
  assert(resid == resid);
  for(int i=0;i<m;i++) p[i] += delta[i];
  return resid;
}

// TODO: could use only one callback for f and J, would be more efficient
static inline double
dt_gauss_newton_cg(
    void (*f_callback)(double *p, double *f, int m, int n, void *data),
    void (*J_callback)(double *p, double *J, int m, int n, void *data),
    double       *p,      // initial paramters, will be overwritten
    const double *t,      // target values
    const int     m,      // number of parameters
    const int     n,      // number of data points
    const double *lb,     // m lower bound constraints
    const double *ub,     // m upper bound constraints
    const int     num_it, // number of iterations
    void         *data)
{
  double *f = alloca(sizeof(double)*n);
  double *J = alloca(sizeof(double)*n*m);
  double resid = 0.0;
  for(int it=0;it<num_it;it++)
  {
    f_callback(p, f, m, n, data);
    J_callback(p, J, m, n, data);
    resid = dt_gauss_newton_cg_step(m, p, n, f, J, t);
    if(resid <= 0.0) return resid;
    for(int i=0;i<m;i++)
      p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
    // fprintf(stderr, "[solve] residual %g\n", resid);
    if(resid < 1e-30) return resid;
  }
  return resid;
}

static inline double
dt_adam(
    void (*f_callback)(double *p, double *f, int m, int n, void *data),
    void (*J_callback)(double *p, double *J, int m, int n, void *data),
    double       *p,      // initial paramters, will be overwritten
    const double *t,      // target values
    const int     m,      // number of parameters
    const int     n,      // number of data points
    const double *lb,     // m lower bound constraints
    const double *ub,     // m upper bound constraints
    const int     num_it, // number of iterations
    void         *data,
    double        eps,    // 10e-8 to avoid squared gradient estimation to drop to zero
    double        beta1,  // 0.9 decay rate of gradient
    double        beta2,  // 0.99 decay rate of squared gradient
    double        alpha,  // 0.001 learning rate
    int          *abort)  // external user flag to abort asynchronously. can be 0.
{
  double *f  = alloca(sizeof(double)*n);
  double *bp = alloca(sizeof(double)*m);   // best p seen so far
  double *J  = alloca(sizeof(double)*n*m);
  double *mt = alloca(sizeof(double)*n*m); // averaged gradient
  double vt   = 0.0;     // sum of squared past gradients
  double best = DBL_MAX; // best loss seen so far
  memset(mt, 0, sizeof(double)*m*n);
  assert(n == 1); // this only works for a single loss value

  for(int it=1;it<num_it+1;it++)
  {
    f_callback(p, f, m, n, data);
    J_callback(p, J, m, n, data);
    if(f[0] < best)
    {
      best = f[0];
      memcpy(bp, p, sizeof(double)*m);
    }
    double sumJ2 = 0.0;
    for(int k=0;k<n*m;k++) sumJ2 += J[k]*J[k];
    vt = beta2 * vt + (1.0-beta2) * sumJ2;
    for(int k=0;k<n*m;k++)
      mt[k] = beta1 * mt[k] + (1.0-beta1) * J[k];

    double corr_m = 1.0/(1.0-pow(beta1, it));
    double corr_v = sqrt(vt / (1.0-pow(beta2, it)));
    if(!(corr_v > 0.0)) corr_v = 1.0;
    if(!(corr_m > 0.0)) corr_m = 1.0;

    for(int k=0;k<m;k++)
      p[k] -= mt[k]*corr_m * alpha / (corr_v + eps);
    for(int i=0;i<m;i++)
      p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
    // for(int i=0;i<m;i++) p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
    // fprintf(stderr, "[solve] adam: p: ");
    // for(int i=0;i<m;i++) fprintf(stderr, "%g\t", p[i]);
    // fprintf(stderr, "\n");
    // fprintf(stderr, "[solve] adam: m: ");
    // for(int i=0;i<m;i++) fprintf(stderr, "%g ", mt[i]);
    // fprintf(stderr, "\n");
    fprintf(stderr, "[solve %d/%d] adam: loss %g %a best %g\n", it, num_it, f[0], f[0], best);
    // if(f[0] <= 0.0) return f[0];
    if(abort && *abort) break;
  }
  if(best < f[0])
  {
    memcpy(p, bp, sizeof(double)*m);
    return best;
  }
  return f[0];
}
