#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

// TODO: the plan:
// * construct even transform -2..2 (NDC)
//   * fourier needs periodic/cosine only
//   * legendre may be better, degree 6 only even Pk: k=0,2,4,6
// * apply to c/a via atomic uniform accum

static inline double
legendre_P(    // evaluate the legendre orthonormal basis
    int n,     // degree
    double x)  // |x|<1
{
  const double norm[] = {1, 1, 1, 1, 1, 1, 1, 1, 1};
  // const double norm[] = {
  //   1,
  //   0,
  //   0.000773453,
  //   0,
  //   0.00151163,
  //   0,
  //   3.16736e-05,
  // };
  switch(n)
  { // not very elegant, could use some recursive definition instead
    case 0: return 1.0 / norm[n];
    case 1: return x / norm[n];
    case 2: return 0.5   * (3.0*x*x - 1.0)/norm[n];
    case 3: return 0.5   * (5.0*x*x*x - 3.0*x)/norm[n];
    case 4: return 1.0/8.0  * (35.0*x*x*x*x - 30.0*x*x + 3.0)/norm[n];
    case 5: return 1.0/8.0  * (63*pow(x,5.0) - 70.0*x*x*x + 15.0*x)/norm[n];
    case 6: return 1.0/16.0 * (231.0*pow(x,6.0) - 315.0*x*x*x*x + 105*x*x - 5.0)/norm[n];
    default: return 0.0; // XXX only up to degree 6
  }
}


// test case to match some periodic 1d function using fourier moments.
int main(int argc, char *argv[])
{
  srand48(666);
  const int N = 1000;
  const int M = 7;
  assert(M<=50);
  double a[50] = {0.0};
  double b[50] = {0.0};
  for(int i=0;i<N;i++)
  {
    // double x = -1.0 + 2.0 * fmod(i/(N-1.0));// + drand48()*0.1, 1.0);
    // double x = -1.0 + 2.0 * ((i+0.5)/(double)N);
    double x = -1.0 + 2.0 * fmod((i+0.5)/(double)N + drand48()*0.1, 1.0);
    // double f = 1.0-cos(2.0*M_PI*x) + drand48()*0.1;
    double f = 0.1*pow(x,2.0) - 0.001*pow(x,4.0) + 0.0001*pow(x,6.0);
    // double f = 1.0; // for normalisation

#if 0 // fourier is orthonormal
    // now project onto our fourier expansion:
    // eq (5,6) in fourier opacity OIT:
    for(int k=0;k<M;k++)
    {
      // TODO: use the recurrence relation from the paper
      a[k] += 2.0 * f * cos(2.0*M_PI*k*x)/N;
      b[k] += 2.0 * f * sin(2.0*M_PI*k*x)/N;
    }
#else // legendre polynomials
    // nb it is enough to integrate 0..1 and collect only even/odd Pk if f is even/odd.
    // for even functions that go through 0/0, we still need P0 because the k>0 have
    // offsets too which need to be corrected to reach zero.
    // for(int k=0;k<=6;k++)
    for(int k=0;k<=6;k+=2)
      a[k] += (2.0*k + 1.0)/2.0 * f * legendre_P(k, x) * 2.0 / N;
#endif
    fprintf(stdout, "%g %g\n", x, f);
  }
  // for(int k=0;k<M;k++)
  //   fprintf(stderr, "%d %g\n", k, a[k]);
  // exit(0);
  // now plot the thing:
  for(int i=0;i<500;i++)
  {
    double x = -1.0 + 2.0 * i/500.0;
#if 0
    float f = a[0]/2.0;
    for(int k=1;k<M;k++)
    {
      // TODO: recurrence relation for sin/cos k+1?
      // sin((n + 1)θ) = sin(nθ) cos(θ) + cos(nθ) sin(θ)
      // cos((n + 1)θ) = cos(nθ) cos(θ) − sin(nθ) sin(θ)
      f += a[k] * cos(2.0*M_PI*k*x)
        ;//+  b[k] * sin(2.0*M_PI*k*x); // cosine transform only?
    }
#else
    double f = 0.0;//a[0];// / 2.0;
    // for(int k=1;k<=6;k++)//=2)
    for(int k=0;k<=6;k+=2)
      f += a[k] * legendre_P(k, x);
#endif
    fprintf(stderr, "%g %g\n", x, f);
  }
  exit(0);
}
