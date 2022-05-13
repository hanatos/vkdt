// load spectrum as much as we can
// match model to it

#include "core/core.h"
#include "core/solve.h"
#include "spectrum.h"
#include "xrand.h"
#include <stdlib.h>
#include <stdio.h>
// #define USE_LEVMAR
#ifdef USE_LEVMAR
#include "levmar-2.6/levmar.h"
#endif

static uint32_t sp_cnt;
static double   sp_buf[100][4];
static uint32_t num_coeff = 9;

void print_ssf(
    double *p)
{
  for(int i=360;i<=830;i+=5)
    fprintf(stdout, "%d %g %g %g\n",
        i,
        sigmoid(poly(p+0*num_coeff, (i-360.0)/(830.0-360.0), num_coeff)),
        sigmoid(poly(p+1*num_coeff, (i-360.0)/(830.0-360.0), num_coeff)),
        sigmoid(poly(p+2*num_coeff, (i-360.0)/(830.0-360.0), num_coeff)));
}

void loss0(
    double *p, // parameters: num_coeff * 3 for camera cfa spectra
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points (=1)
    void   *data)
{
    // fprintf(stderr, "[loss] p = %g %g %g %g %g \n", p[0], p[1], p[2], p[3], p[4]);
  double l = 0.0;
  for(int i=0;i<sp_cnt;i++)
  {
    double rgb[3] = {
      /*sigmoid*/(poly(p+0*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
      /*sigmoid*/(poly(p+1*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
      /*sigmoid*/(poly(p+2*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
    };
    // fprintf(stderr, "p = %g %g %g %g %g %g \n", p[0], p[1], p[2], p[3], p[4], p[5]);
    // fprintf(stderr, "lambda %g val %g %g\n",sp_buf[i][0], rgb[0], sp_buf[i][1]);
    l += fabs(rgb[0] - inv_sigmoid(sp_buf[i][1]));//*(rgb[0] - inv_sigmoid(sp_buf[i][1]));
    l += fabs(rgb[1] - inv_sigmoid(sp_buf[i][2]));//*(rgb[1] - inv_sigmoid(sp_buf[i][2]));
    l += fabs(rgb[2] - inv_sigmoid(sp_buf[i][3]));//*(rgb[2] - inv_sigmoid(sp_buf[i][3]));
  }
  x[0] = l;
}

void loss0_dif(
    double *p,   // parameters: num_coeff * 3 for camera cfa spectra
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters (=num_coeff*3)
    int     n,   // number of data points (=1)
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1, X2;
    double p2[m];
    const double h = 1e-10 + xrand()*1e-3;//1e-10;
    memcpy(p2, p, sizeof(p2));
    p2[j] += h;
    loss0(p2, &X1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] -= h;
    loss0(p2, &X2, m, n, data);
    jac[j] = (X1 - X2) / (2.0*h);
    // fprintf(stderr, "J %d = %g\n", j, jac[j]);
  }
}


void loss(
    double *p, // parameters: num_coeff * 3 for camera cfa spectra
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points (=1)
    void   *data)
{
    // fprintf(stderr, "[loss] p = %g %g %g %g %g \n", p[0], p[1], p[2], p[3], p[4]);
  double l = 0.0;
  for(int i=0;i<sp_cnt;i++)
  {
    double rgb[3] = {
      sigmoid(poly(p+0*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
      sigmoid(poly(p+1*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
      sigmoid(poly(p+2*num_coeff, (sp_buf[i][0]-360.0)/(830.0-360.0), num_coeff)),
    };
    // fprintf(stderr, "p = %g %g %g %g %g %g \n", p[0], p[1], p[2], p[3], p[4], p[5]);
    // fprintf(stderr, "lambda %g val %g %g\n",sp_buf[i][0], rgb[0], sp_buf[i][1]);
    l += fabs(rgb[0] - (sp_buf[i][1]));//*(rgb[0] - (sp_buf[i][1]));
    l += fabs(rgb[1] - (sp_buf[i][2]));//*(rgb[1] - (sp_buf[i][2]));
    l += fabs(rgb[2] - (sp_buf[i][3]));//*(rgb[2] - (sp_buf[i][3]));
  }
  x[0] = l;
}

void loss_dif(
    double *p,   // parameters: num_coeff * 3 for camera cfa spectra
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters (=num_coeff*3)
    int     n,   // number of data points (=1)
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1, X2;
    double p2[m];
    const double h = 1e-7;//1e-10;// + xrand()*1e-5;//1e-10;
    memcpy(p2, p, sizeof(p2));
    p2[j] += h;
    loss(p2, &X1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] -= h;
    loss(p2, &X2, m, n, data);
    jac[j] = (X1 - X2) / (2.0*h);
    // fprintf(stderr, "J %d = %g\n", j, jac[j]);
  }
}

int main(int argc, char *argv[])
{
  const char *model = "Canon EOS 5D Mark II";
  for(int k=1;k<argc;k++)
    model = argv[1];

  sp_cnt = spectrum_load(model, sp_buf);

  double resid = FLT_MAX;
  const int num_it = 10000000;
int batch = 0, batch_cnt = 0;
  double target[1] = {0.0};
  double cfa[30] = {0.0};
  double lb[30] = {0.0};
  double ub[30] = {0.0};
  for(int k=0;k<3*num_coeff;k++) lb[k] = -FLT_MAX;//50;
  for(int k=0;k<3*num_coeff;k++) ub[k] =  FLT_MAX;//50;
  int e = num_coeff-3; // quadratic term
  // cfa[e+0] = cfa[e+num_coeff] = cfa[e+2*num_coeff] = -3e-3;
  cfa[e+0] = cfa[e+num_coeff] = cfa[e+2*num_coeff] = -3;
  e = num_coeff-1; // linear term
  cfa[e+0] = cfa[e+num_coeff] = cfa[e+2*num_coeff] = 1.0;
#ifdef USE_LEVMAR
    double info[LM_INFO_SZ] = {0};
    double opts[LM_OPTS_SZ] = {
      // init-mu    eps Jte   eps Dp    eps err   eps diff
      0.2, 1E-15, 1E-40, 1E-15, 1e-5};//LM_DIFF_DELTA};

#if 0
    if(der)
    {
      dlevmar_bc_der(
          lm_callback, lm_jacobian, cfa, target, 3*num_coeff, 2*num,
          lb, ub, NULL, // dscl, // diagonal scaling constants (?)
          num_it, opts, info, NULL, NULL, data);
    }
    else
#endif
    {
      dlevmar_bc_dif(
          loss, cfa, target, 3*num_coeff, 1,
          lb, ub, NULL, // dscl, // diagonal scaling constants (?)
          num_it, opts, info, NULL, NULL, 0);
    }
    // fprintf(stderr, "    ||e||_2, ||J^T e||_inf,  ||Dp||_2, mu/max[J^T J]_ii\n");
    // fprintf(stderr, "info %g %g %g %g\n", info[1], info[2], info[3], info[4]);
    fprintf(stderr, "\r[vkdt-mkidt] batch %d/%d it %04d/%d reason %g resid %g -> %g                ",
        batch+1, batch_cnt, (int)info[5], num_it, info[6], info[0], info[1]);
    fprintf(stdout, "%g %g\n", info[0], info[1]);
#else
    // fprintf(stdout, "%g ", resid);
    // resid = dt_gauss_newton_cg(
    //     loss, loss_dif,
    //     cfa, target, 3*num_coeff, 1,
    //     lb, ub, num_it, 0);
#if 0
    resid = dt_adam(
        loss0, loss0_dif,
        cfa, target, 3*num_coeff, 1,
        lb, ub, num_it, 0, 1e-8, 0.9, 0.99, 0.5, 0);
#endif
    resid = dt_adam(
        loss, loss_dif,
        cfa, target, 3*num_coeff, 1,
        lb, ub, num_it, 0, 1e-8, 0.9, 0.99, 0.5, 0);
    fprintf(stderr, "\r[vkdt-matchssf] batch %d/%d resid %g               ",
        batch+1, batch_cnt, resid);
    // fprintf(stdout, "%g\n", resid); // write convergence history
    for(int k=0;k<3;k++)
    {
      for(int i=0;i<num_coeff;i++)
        fprintf(stderr, "%g ", cfa[k*num_coeff+i]);
      fprintf(stderr, "\n");
    }
    print_ssf(cfa);
fprintf(stderr, "\n");
#endif
  exit(0);
}
