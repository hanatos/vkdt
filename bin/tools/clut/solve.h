#pragma once

#include <stdint.h>
#include <alloca.h>
#include <assert.h>
#include <string.h>

// conjugate gradient solve:
static inline double
conj_grad(const double *A, const double *b, double *x, const int m)
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
    assert(alpha == alpha);
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
gauss_newton_cg_step(
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
  double resid = conj_grad(A, b, delta, m);
  assert(resid == resid);
  for(int i=0;i<m;i++) p[i] += delta[i];
  return resid;
}

// TODO: could use only one callback for f and J, would be more efficient
static inline double
gauss_newton_cg(
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
    resid = gauss_newton_cg_step(m, p, n, f, J, t);
    if(resid < 0.0) return resid;
    for(int i=0;i<m;i++)
      p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
  }
  return resid;
}
