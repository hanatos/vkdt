// converted some old code to use our cie1931 arrays
//
// this will precompute a map suitable to reconstruct MacAdam-style box spectra
// from tristimulus input.
// the map contains three numbers per pixel: maximum brightness (X+Y+Z),
// lambda0 and lambda1. the two wavelengths are the locations of the rising
// and the falling edge, respectively. if the falling edge comes before the
// rising one, the spectrum is a dip instead of a peak.
#include "../cie1931_c.h"
#include "macadam.h"
#include "clip.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(int argc, char *argv[])
{
  const int max_l = CIE_SAMPLES*2;
  float *cum_xyz = malloc(sizeof(float)*3*max_l);
  cum_xyz[0] = cie_x[0]*5; // 5nm intervals
  cum_xyz[1] = cie_y[0]*5;
  cum_xyz[2] = cie_z[0]*5;
  const float inc = 1./64.;//1./8.;
  for(int i=1;i<max_l;i++)
  {
    const int ii = i >= CIE_SAMPLES ? i - CIE_SAMPLES: i;
    cum_xyz[3*i+0] = cum_xyz[3*(i-1)+0] + cie_x[ii]*5;
    cum_xyz[3*i+1] = cum_xyz[3*(i-1)+1] + cie_y[ii]*5;
    cum_xyz[3*i+2] = cum_xyz[3*(i-1)+2] + cie_z[ii]*5;
  }
  const int num_mips = 5;
  int res[num_mips+1];
  res[0] = 1024;
  float *mip[num_mips];
  for(int k=0;k<num_mips;k++)
  {
    mip[k] = malloc(sizeof(float)*4*res[k]*res[k]);
    memset(mip[k], 0, sizeof(float)*4*res[k]*res[k]);
    res[k+1] = res[k]/2;
  }

  // enumerate all possible box spectra in the sense of [MacAdam 1935],
  // all wavelengths l: l0 <= l <= l1 are s(l) = 1, 0 else:
  for(float w0=0;w0<=max_l/2-1;w0+=inc)
  {
    for(float w1=w0+inc;w1<=max_l-2;w1+=inc)
    {
      // compute xy chromaticities:
      // const int l0 = w0, l1 = w1;
      // const float f0 = w0-l0, f1 = w1-l1;
      const float lambda0 = CIE_LAMBDA_MIN + w0 * 5.0;
      const float lambda1 = CIE_LAMBDA_MIN + w1 * 5.0 > CIE_LAMBDA_MAX ?
         CIE_LAMBDA_MIN + (w1 - CIE_SAMPLES + 1)*5.0 :
         CIE_LAMBDA_MIN + w1 * 5.0;
      assert(lambda0 >= CIE_LAMBDA_MIN);
      assert(lambda0 <= CIE_LAMBDA_MAX);
      assert(lambda1 >= CIE_LAMBDA_MIN);
      assert(lambda1 <= CIE_LAMBDA_MAX);
#if 0
      const float X = (1.0f-f1)*cum_xyz[3*l1+0] + f1*cum_xyz[3*l1+3+0] - (1.0f-f0)*cum_xyz[3*l0+0] - f0*cum_xyz[3*l0+3+0];
      const float Y = (1.0f-f1)*cum_xyz[3*l1+1] + f1*cum_xyz[3*l1+3+1] - (1.0f-f0)*cum_xyz[3*l0+1] - f0*cum_xyz[3*l0+3+1];
      const float Z = (1.0f-f1)*cum_xyz[3*l1+2] + f1*cum_xyz[3*l1+3+2] - (1.0f-f0)*cum_xyz[3*l0+2] - f0*cum_xyz[3*l0+3+2];
#else

      double X, Y, Z;
      X = Y = Z = 0.0;
      for(int l=0;l<5*CIE_SAMPLES;l++)
      {
        const float ll = l / (5.0f*CIE_SAMPLES - 1.0f);
        const float lambda = CIE_LAMBDA_MIN * (1.0f-ll) + CIE_LAMBDA_MAX * ll;
        float p = 0.0f;
        if(lambda0 < lambda1)
        { // peak
          if(lambda > lambda0 && lambda <= lambda1) p = 1.0f;
        }
        else
        { // dip
          if(lambda <= lambda1 || lambda > lambda0) p = 1.0f;
        }
        X += p * cie_x[l/5] * 1.0f/106.89;//CIE_SAMPLES;
        Y += p * cie_y[l/5] * 1.0f/106.89;//CIE_SAMPLES;
        Z += p * cie_z[l/5] * 1.0f/106.89;//CIE_SAMPLES;
      }
#endif

      const float b = X+Y+Z;
      // fprintf(stderr, "b=%g = %g +%g +%g\n", b, X, Y, Z);
      const float x = X/b;
      const float y = Y/b;
      // fprintf(stderr, "x y %g %g\n", x, y);
      // exit(0);
      // rasterize into map
      // if(b <= 3.0f && b > 1e-4f && x > 0 && y > 0)
      if(b > 1e-4f && x > 0 && y > 0)
      {
        for(int k=0;k<num_mips;k++)
        {
          const int i = x*res[k], j = y*res[k];
          if(i>=0&&i<res[k]&&j>=0&&j<res[k])
          {
            float *v = mip[k] + 4*(j*res[k]+i);
            // const float n = v[3];
            // const float t0 = n/(n+1.0), t1 = 1.0/(n+1.0);
            const float t0 = 0.0f, t1 = 1.0f;
            // fprintf(stderr, "mip %d raster %g %g %g\n", k, b, lambda0, lambda1);
            v[0] = t0*v[0] + t1*b;
            v[1] = t0*v[1] + t1*lambda0;
            v[2] = t0*v[2] + t1*lambda1;
            v[3] ++ ;
            assert(v[1] >= CIE_LAMBDA_MIN);
            assert(v[1] <= CIE_LAMBDA_MAX);
            assert(v[2] >= CIE_LAMBDA_MIN);
            assert(v[2] <= CIE_LAMBDA_MAX);
          }
        }
      }
    }
  }

#if 1
  // superbasic hole filling, only needed with large increments.
  // low increments will have supersampled this anyways already.
  for(int j=0;j<res[0];j++)
  {
    for(int i=0;i<res[0];i++)
    {
      const float x = i / (float)res[0];
      const float y = j / (float)res[0];
      if(spectrum_outside(x, y))
      {
        for(int c=0;c<3;c++)
          mip[0][4*(j*res[0]+i)+c] = 0.0f; // not a colour
      }
      else if(mip[0][4*(j*res[0]+i)] == 0)
      {
        int ii=i/2,jj=j/2;
        for(int k=1;k<num_mips;k++,ii/=2,jj/=2)
        {
          if(mip[k][4*(jj*res[k]+ii)] > 0)
          {
            for(int c=0;c<3;c++)
              mip[0][4*(j*res[0]+i)+c] = mip[k][4*(jj*res[k]+ii)+c];
            break;
          }
        }
      }
    }
  }
#endif

  FILE *f = fopen("macadam.pfm", "wb");
  if(f)
  {
    const int m = 0;
    fprintf(f, "PF\n%d %d\n-1.0\n", res[m], res[m]);
    for(int k=0;k<res[m]*res[m];k++)
      fwrite(mip[m]+4*k, sizeof(float)*3, 1, f);
    fclose(f);
  }

#if 1 // test
  MacAdam_t mad;
  MacAdam_init(&mad, "macadam.pfm");
  const float xyz[] = {1, 1, 1};//0.99, 0.98, 0.98};
  for(int i=0;i<95;i++)
  {
    float lambda = 360.0f+i*5.0f;
    fprintf(stderr, "%g %g\n", lambda, MacAdam_eval(&mad, lambda, xyz));
  }
  MacAdam_cleanup(&mad);
#endif

  free(cum_xyz);
  exit(0);
}
