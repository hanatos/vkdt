// converted some old code to use our cie1931 arrays
//
// this will precompute a map suitable to reconstruct MacAdam-style box spectra
// from tristimulus input.
// the map contains three numbers per pixel: maximum brightness (X+Y+Z),
// lambda0 and lambda1. the two wavelengths are the locations of the rising
// and the falling edge, respectively. if the falling edge comes before the
// rising one, the spectrum is a dip instead of a peak.
#define CIE_SAMPLES 95
#define CIE_LAMBDA_MIN 360.0
#define CIE_LAMBDA_MAX 830.0
#define CIE_FINE_SAMPLES ((CIE_SAMPLES - 1) * 3 + 1)
#include "shared/cie1931.h"
#include "core/inpaint.h"
#include "core/clip.h"
#include "core/half.h"
#include "core/threads.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const int res = 1024;
const int max_l = CIE_SAMPLES*2;
const int incres = 8.0;//64.0;

void parallel_run(uint32_t item, void *data)
{
  // enumerate all possible box spectra in the sense of [MacAdam 1935],
  // all wavelengths l: l0 <= l <= l1 are s(l) = 1, 0 else:
  const int iw0 = item;
  float *buf = data;
  for(int iw1=iw0+1;iw1<=incres*(max_l-2);iw1++)
  {
    const float w0 = iw0/(float)incres;
    const float w1 = iw1/(float)incres;
    // compute xy chromaticities:
    // const int l0 = w0, l1 = w1;
    // const float f0 = w0-l0, f1 = w1-l1;
    const float lambda0 = CIE_LAMBDA_MIN + w0 * 5.0;
    const float lambda1 = CIE_LAMBDA_MIN + w1 * 5.0 > CIE_LAMBDA_MAX ?
       CIE_LAMBDA_MIN + (w1 - CIE_SAMPLES + 1)*5.0 :
       CIE_LAMBDA_MIN + w1 * 5.0;

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
      X += p * cie_x[l/5] * 1.0f/106.89;
      Y += p * cie_y[l/5] * 1.0f/106.89;
      Z += p * cie_z[l/5] * 1.0f/106.89;
    }

    const float b = X+Y+Z;
    const float x = X/b;
    const float y = Y/b;
    // rasterize into map
    if(b > 1e-4f && x > 0 && y > 0)
    {
      const int i = x*res+0.5f, j = y*res+0.5f;
      if(i>=0&&i<res&&j>=0&&j<res)
      {
        float *v = buf + 4*(j*res+i);
        // const float n = v[3];
        // const float t0 = n/(n+1.0), t1 = 1.0/(n+1.0);
        const float t0 = 0.0f, t1 = 1.0f;
        v[0] = t0*v[0] + t1*b;
        v[1] = t0*v[1] + t1*lambda0;
        v[2] = t0*v[2] + t1*lambda1;
        v[3] ++ ;
      }
    }
  }
}

int main(int argc, char *argv[])
{
  float *buf = calloc(sizeof(float), 4*res*res);

  threads_global_init();
  const uint32_t work_item_cnt = incres * (max_l/2-1)+1;
#if 0
  const int nt = threads_num();
  int taskid = -1;
  for(int i=0;i<nt;i++)
    taskid = threads_task(work_item_cnt, taskid, buf, parallel_run, 0);
  threads_wait(taskid);
#else // single threaded version:
  for(int k=0;k<work_item_cnt;k++)
    parallel_run(k, buf);
#endif

  // inpaint/hole filling
  dt_inpaint_buf_t inpaint_buf = {
    .dat = buf,
    .wd  = res,
    .ht  = res,
    .cpp = 4,
  };
  dt_inpaint(&inpaint_buf);

  // clear out of gamut values again
  for(int j=0;j<res;j++)
    for(int i=0;i<res;i++)
      if(dt_spectrum_outside(
            (i+.5) / (float)res,
            (j+.5) / (float)res))
        for(int c=0;c<3;c++)
          buf[4*(j*res+i)+c] = 0.0f;

  // blur 5x5 to smooth over cmf resolution
  float *smooth = calloc(sizeof(float), res*res);
  for(int j=0;j<res;j++) for(int i=0;i<res;i++)
  {
    const float wg[] = {1.0f, 4.0f, 6.0f, 4.0f, 1.0f};
    float weight = 0.0f;
    // for(int jj=-2;jj<=2;jj++) for(int ii=-2;ii<=2;ii++)
    for(int jj=-4;jj<=4;jj+=2) for(int ii=-4;ii<=4;ii+=2) // even smoother
    {
      if(j+jj >= 0 && j+jj < res && i+ii >= 0 && i+ii < res)
      {
        // float w = wg[jj+2]*wg[ii+2];
        float w = wg[jj/2+2]*wg[ii/2+2];
        smooth[j*res+i] += w * buf[4*((j+jj)*res+i+ii)];
        weight += w;
      }
    }
    if(weight > 0.0f) smooth[j*res+i] /= weight;
  }

  // write 1 channel half lut:
  uint32_t size = sizeof(uint16_t)*res*res;
  uint16_t *b16 = malloc(size);
  for(int k=0;k<res*res;k++)
    b16[k] = float_to_half(smooth[k]);
  typedef struct header_t
  {
    uint32_t magic;
    uint16_t version;
    uint8_t  channels;
    uint8_t  datatype;
    uint32_t wd;
    uint32_t ht;
  }
  header_t;
  header_t head = (header_t) {
    .magic    = 1234,
    .version  = 2,
    .channels = 1,
    .datatype = 0,
    .wd       = res,
    .ht       = res,
  };
  FILE *f = fopen("macadam.lut", "wb");
  if(f)
  {
    fwrite(&head, sizeof(head), 1, f);
    fwrite(b16, size, 1, f);
    fclose(f);
  }
#if 0 // debug, can look at this with eu:
  FILE *pfm = fopen("macadam.pfm", "wb");
  if(pfm)
  {
    fprintf(pfm, "PF\n%d %d\n-1.0\n", res, res);
    for(int k=0;k<res*res;k++) for(int c=0;c<3;c++)
      fwrite(smooth+k, sizeof(float), 1, pfm);
    fclose(pfm);
  }
#endif
  free(b16);
  free(smooth);
  threads_global_cleanup();

  exit(0);
}
