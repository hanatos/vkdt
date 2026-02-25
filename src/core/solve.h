#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <math.h>
#include <float.h>

#include "core.h"
#include "../tools/clut/src/xrand.h"

// conjugate gradient solve:
static inline double
dt_conj_grad(const double *A, const double *b, double *x, const int m)
{
  for(int j=0;j<m;j++) x[j] = 0.0;
  double *r = malloc(sizeof(double)*m);
  for(int j=0;j<m;j++) r[j] = b[j];

  double *p = malloc(sizeof(double)*m);
  for(int j=0;j<m;j++) p[j] = r[j];

  double rsold = 0.0;
  for(int j=0;j<m;j++) rsold += r[j]*r[j];

  double *Ap = malloc(sizeof(double)*m);
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
    if(!(alpha == alpha)) { free(r); free(p); free(Ap); return 0.1; }
    for(int j=0;j<m;j++)
      x[j] += alpha * p[j];
    for(int j=0;j<m;j++)
      r[j] -= alpha * Ap[j];
    double rsnew = 0.0;
    for(int j=0;j<m;j++) rsnew += r[j]*r[j];
    if(sqrt(rsnew) < 2e-6) { free(r); free(p); free(Ap); return sqrt(rsnew); }
    if(rsnew > rsold) { free(r); free(p); free(Ap); return sqrt(rsnew); }
    for(int j=0;j<m;j++) p[j] = r[j] + rsnew / rsold * p[j];
    rsold = rsnew;
  }
  free(r); free(p); free(Ap);
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
  double *b = malloc(sizeof(double)*m);
  memset(b, 0, sizeof(double)*m);
  for(int i=0;i<m;i++)
    for(int j=0;j<n;j++)
      b[i] += J[m*j + i] * (t[j] - f[j]);

  // compute dense square matrix Jt J
  double *A = malloc(sizeof(double)*m*m);
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

  double *delta = malloc(sizeof(double)*m);
  double resid = dt_conj_grad(A, b, delta, m);
  assert(resid == resid);
  for(int i=0;i<m;i++) p[i] += delta[i];
  free(b); free(A); free(delta);
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
  double *bp = malloc(sizeof(double)*m);   // best p seen so far
  double *f = malloc(sizeof(double)*n);
  double *J = malloc(sizeof(double)*n*m);
  double resid = 0.0;
  double best = DBL_MAX;
  for(int it=0;it<num_it;it++)
  {
    f_callback(p, f, m, n, data);
    J_callback(p, J, m, n, data);
    if(f[0] < best)
    {
      best = f[0];
      memcpy(bp, p, sizeof(double)*m);
    }
    resid = dt_gauss_newton_cg_step(m, p, n, f, J, t);
    // if(resid <= 0.0) return resid;
    for(int i=0;i<m;i++)
      p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
    fprintf(stderr, "[solve %d/%d] gauss: loss %g\r", it, num_it, f[0]);
    if(resid < 1e-30)
    {
      free(bp); free(f); free(J);
      return resid;
    }
  }
#if 1
  if(best < f[0])
  {
    memcpy(p, bp, sizeof(double)*m);
    free(bp); free(f); free(J);
    return best;
  }
#endif
  free(bp); free(f); free(J);
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
    double        eps,    // 1e-8  to avoid squared gradient estimation to drop to zero
    double        beta1,  // 0.9   decay rate of gradient
    double        beta2,  // 0.999 decay rate of squared gradient
    double        alpha,  // 0.001 learning rate
    int          *abort)  // external user flag to abort asynchronously. can be 0.
{
  double *f  = malloc(sizeof(double)*n);
  double *bp = malloc(sizeof(double)*m);   // best p seen so far
  double *J  = malloc(sizeof(double)*n*m);
  double *mt = malloc(sizeof(double)*n*m); // averaged gradient (1st moment)
  double *vt = malloc(sizeof(double)*n*m); // past gradient second moment
  double best = DBL_MAX; // best loss seen so far
  memset(mt, 0, sizeof(double)*m*n);
  memset(vt, 0, sizeof(double)*m*n);
  assert(n == 1); // this only works for a single loss value

  // int bad = 0;

  for(int it=1;it<num_it+1;it++)
  {
    f_callback(p, f, m, n, data);
    J_callback(p, J, m, n, data);
    if(f[0] < best)
    {
      best = f[0];
      memcpy(bp, p, sizeof(double)*m);
    }
#if 0
    else if(bad++ > 200)
    {
      memcpy(p, bp, sizeof(double)*m);
    }
#endif
    for(int k=0;k<n*m;k++) vt[k] = beta2 * vt[k] + (1.0-beta2) * J[k] * J[k];
    for(int k=0;k<n*m;k++) mt[k] = beta1 * mt[k] + (1.0-beta1) * J[k];

    double corr_m = 1.0/(1.0-pow(beta1, it));
    double corr_v = 1.0/(1.0-pow(beta2, it));
    if(!(corr_v > 0.0)) corr_v = 1.0;
    if(!(corr_m > 0.0)) corr_m = 1.0;

    for(int k=0;k<m;k++)
      p[k] -= mt[k]*corr_m * alpha / (sqrt(vt[k]*corr_v) + eps);
    for(int i=0;i<m;i++)
      p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);

    // for(int i=0;i<m;i++) p[i] = fminf(fmaxf(p[i], lb[i]), ub[i]);
    // fprintf(stderr, "\n[solve] adam: p: ");
    // for(int i=0;i<m;i++) fprintf(stderr, "%g\t", p[i]);
    // fprintf(stderr, "\n");
    // fprintf(stderr, "[solve] adam: m: ");
    // for(int i=0;i<m;i++) fprintf(stderr, "%g ", mt[i]);
    // fprintf(stderr, "\n");
    fprintf(stderr, "[solve %d/%d] adam: loss %g best %g\r", it, num_it, f[0], best);
    // if(f[0] <= 0.0) return f[0];
    if(abort && *abort) break;
  }
  fprintf(stderr, "\n");
#if 1 // for stochastic gradient descent this is questionable. may only return whatever happened to be good for the subset seen at the time:
  if(best < f[0])
  {
    memcpy(p, bp, sizeof(double)*m);
    free(f); free(bp); free(J); free(mt); free(vt);
    return best;
  }
#endif
  double res = f[0];
  free(f); free(bp); free(J); free(mt); free(vt);
  return res;
}

/*! A basic implementation of the method of Nelder and Mead for minimization of
  an objective function. The implementation follows the description in their
  paper exactly and uses the parameter values described there:
  John A. Nelder and Roger Mead 1965, A Simplex Method for Function
  Minimization, The Computer Journal 7:4, Oxford Academic, p. 308-313,
https://doi.org/10.1093/comjnl/7.4.308
returns the minimum objective function.
implemented by christoph peters. */
#define Float double
static inline Float
dt_nelder_mead(
    Float         *param,        // initial parameter vector, will be overwritten with result
    int            dim,          // dimensionality of the problem, i.e. number of parameters
    const int      num_it,       // number of iterations
    Float        (*objective)(Float *param, void *data),
    const double  *lb,           // m lower bound constraints
    const double  *ub,           // m upper bound constraints
    void          *data,         // data pointer passed to objective function
    int           *user_abort)   // check within iteration whether we should abort, if not null
{
  const Float reflection_coeff  = (Float)1.0; // The reflection coefficient alpha
  const Float contraction_coeff = (Float)0.5; // The contraction coefficient beta
  const Float expansion_coeff   = (Float)2.0; // The expansion coefficient gamma
  const Float shrink_coeff      = (Float)0.5; // The shrinking coefficient (implicitly 0.5 in the paper)
  Float *simplex_buf = malloc(sizeof(Float)*dim*(dim+1));
  Float (*simplex)[dim] = (Float (*)[dim])simplex_buf; // Construct the initial simplex using fixed axis-aligned offsets
  memcpy(simplex[0], param, sizeof(Float)*dim);
  for(uint32_t j = 0; j != dim; j++)
  {
    memcpy(simplex[j + 1], param, sizeof(Float)*dim);
    simplex[j + 1][j] += 0.05;
  }
  // Evaluate the objective at all simplex vertices:
  Float *simplex_value = malloc(sizeof(Float)*(dim+1));
  for(uint32_t j = 0; j < dim + 1; j++) simplex_value[j] = objective(simplex[j], data);

  Float *centroid   = malloc(sizeof(Float)*dim);
  Float *reflected  = malloc(sizeof(Float)*dim);
  Float *expanded   = malloc(sizeof(Float)*dim);
  Float *contracted = malloc(sizeof(Float)*dim);
  // We also keep track of the best value ever seen in case it is not part of
  // the simplex (which may be the case in presence of failed expansions)
  Float *best = malloc(sizeof(Float)*dim);
  memcpy(best, simplex[0], sizeof(Float)*dim);
  Float best_value = simplex_value[0];
  int it_best_changed = 0;
  int restarts = 0;
  for(uint32_t i = 0; i < num_it; i++)
  { // Find indices of minimal and maximal value
    fprintf(stderr, "\r[nelder mead] %d/%d best value %g", i, num_it, best_value);
    if(user_abort && *user_abort) break;
    uint32_t min_index = 0, max_index = 0;
    Float min = simplex_value[0], max = simplex_value[0];
    for(uint32_t j = 1; j < dim + 1; j++)
    {
      min_index = (min > simplex_value[j]) ? j : min_index;
      min = (min > simplex_value[j]) ? simplex_value[j] : min;
      max_index = (max < simplex_value[j]) ? j : max_index;
      max = (max < simplex_value[j]) ? simplex_value[j] : max;
    }

    memset(centroid, 0, sizeof(Float)*dim); // compute the centroid of the facet opposite to the worst vertex
    for(uint32_t j = 0; j < dim+1; j++)
      if (j != max_index)
        for(uint32_t k = 0; k < dim; k++)
          centroid[k] += simplex[j][k];
    for(uint32_t k=0;k<dim;k++) centroid[k] *= 1.0 / dim;
    // compute the reflected point
    for(uint32_t k = 0; k < dim; k++) reflected[k] = (1.0 + reflection_coeff) * centroid[k] - reflection_coeff * simplex[max_index][k];
    Float reflected_value = objective(reflected, data);

    if (reflected_value < simplex_value[min_index])
    { // If the reflected point is better than the best vertex, we expand further in that direction
      for(uint32_t k=0;k<dim;k++) expanded[k] = expansion_coeff * reflected[k] + (1.0 - expansion_coeff) * centroid[k];
      Float expanded_value = objective(expanded, data);
      if(expanded_value < simplex_value[min_index])
      { // if the expanded point is also better than the best vertex, we replace the worst vertex by it
        memcpy(simplex[max_index], expanded, sizeof(Float)*dim);
        simplex_value[max_index] = expanded_value;
        if (reflected_value < best_value)
        { // If the reflected value is the best yet, we should not lose it
          memcpy(best, reflected, sizeof(Float)*dim);
          best_value = reflected_value;
          it_best_changed = i;
          restarts = 0;
        }
      }
      else
      { // Otherwise, we have a failed expansion and use the reflected value instead
        memcpy(simplex[max_index], reflected, sizeof(Float)*dim);
        simplex_value[max_index] = reflected_value;
      }
    }
    else
    { // Check whether replacing the worst vertex by the reflected vertex makes some other vertex the worst
      int reflected_still_worst = 1;
      for(uint32_t j = 0; j < dim + 1; j++)
        reflected_still_worst &= (j == max_index || simplex_value[j] < reflected_value);
      if (reflected_still_worst)
      { // If the reflected point is better than the worst vertex, use it anyway
        if (reflected_value < simplex_value[max_index])
        {
          memcpy(simplex[max_index], reflected, sizeof(Float)*dim);
          simplex_value[max_index] = reflected_value;
        }
        // Form a contracted point between worst vertex and centroid
        for(int k=0;k<dim;k++) contracted[k] = contraction_coeff * simplex[max_index][k] + (1.0 - contraction_coeff) * centroid[k];
        Float contracted_value = objective(contracted, data);
        if(contracted_value < simplex_value[max_index])
        { // replace the worst vertex by the contracted point if it makes an improvement
          memcpy(simplex[max_index], contracted, sizeof(Float)*dim);
          simplex_value[max_index] = contracted_value;
        }
        else
        { // otherwise, we shrink all vertices towards the best vertex
          for (uint32_t j = 0; j < dim + 1; ++j)
          {
            if (j != min_index)
            {
              for(int k=0;k<dim;k++) simplex[j][k] = shrink_coeff * simplex[j][k] + (1.0f - shrink_coeff) * simplex[min_index][k];
              simplex_value[j] = objective(simplex[j], data);
            }
          }
        }
      }
      else
      { // At least two vertices are worse than the reflected point, so we replace the worst vertex by it
        memcpy(simplex[max_index], reflected, sizeof(Float)*dim);
        simplex_value[max_index] = reflected_value;
      }
    }

    for(uint32_t j = 0; j != dim+1; j++)
    { // clamp values
      for(uint32_t k = 0; k != dim; k++)
        simplex[j][k] = CLAMP(simplex[j][k], lb[k], ub[k]);
      simplex_value[j] = objective(simplex[j], data);
    }

    if(i > it_best_changed + 50)
    { // ineffective, at least it aborts early
      it_best_changed = i;
      if(restarts++ > 0) break;
#if 0
      memcpy(simplex[0], best, sizeof(Float)*dim);
      for(uint32_t j = 0; j != dim+1; j++)
      {
        for(uint32_t k = 0; k != dim; k++)
          simplex[j][k] = (best[k] + simplex[j][k])*0.5;
        simplex[j][j] += 0.05*restarts;
      }
#endif
    }
  }
  for(uint32_t j = 0; j < dim + 1; j++)
  { // the best value ever observed is either a vertex of the simplex, or it has been stored in best_value
    if (simplex_value[j] < best_value)
    {
      memcpy(best, simplex[j], sizeof(Float)*dim);
      best_value = simplex_value[j];
    }
  }
  memcpy(param, best, sizeof(Float)*dim);
  free(simplex_buf);
  free(simplex_value);
  free(centroid);
  free(reflected);
  free(expanded);
  free(contracted);
  free(best);
  fprintf(stderr, "\n");
  return best_value;
}

// bogus random search, meant to find potentially useful initial values for actual optimisers
static inline double
dt_bogosearch(
    double        *param,        // initial parameter vector, will be overwritten with result
    int            dim,          // dimensionality of the problem, i.e. number of parameters
    const int      num_it,       // number of iterations
    double       (*objective)(double *param, void *data),
    const double  *lb,           // m lower bound constraints
    const double  *ub,           // m upper bound constraints
    void          *data,         // data pointer passed to objective function
    int           *user_abort)   // check within iteration whether we should abort, if not null
{
  double best_loss = DBL_MAX;
  double *best = malloc(sizeof(double)*dim);
  double *curr = malloc(sizeof(double)*dim);
  double tmp;
  for(int i=0;i<num_it;i++)
  {
    if(user_abort && *user_abort) break;
    if(xrand() < 0.5)
      for(int j=0;j<dim;j++)
        curr[j] = lb[j] + (ub[j]-lb[j])*xrand();
    else
      for(int j=0;j<dim;j++)
        curr[j] = lb[j] + (ub[j]-lb[j])*modf((curr[j] - lb[j])/(ub[j]-lb[j]) + 0.004*(0.5-xrand()), &tmp);
    double loss = objective(curr, data);
    if(loss < best_loss)
    {
      memcpy(best, curr, sizeof(double)*dim);
      best_loss = loss;
    }
    if((i % 10) == 0) fprintf(stderr, "\r[bogo search] %d/%d best value %g", i, num_it, best_loss);
  }
  memcpy(param, best, sizeof(double)*dim);
  free(best);
  free(curr);
  fprintf(stderr, "\n");
  return best_loss;
}
