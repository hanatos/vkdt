#include "modules/api.h"
#include "../core/core.h" // for MIN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

void modify_roi_in(
    dt_graph_t *graph,
    dt_module_t *module)
{
  dt_roi_t *r = &module->connector[0].roi;
  r->wd = r->full_wd;
  r->ht = r->full_ht;
  r->x = 0;
  r->y = 0;
  r->scale = 1.0f;
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  // read back uint32_t buffer, fit line to noise, output param a and param b to file with our maker model iso:
  uint32_t *p32 = buf;

  const int wd = module->connector[0].roi.wd;
  // const int ht = module->connector[0].roi.ht; == 4

  // examine pairs of samples and find out whether one position likely leads to
  // an outlier or not.
  int valid_cnt = 0;
  int *valid = malloc(wd * sizeof(int));
  for(int i=0;i<wd;i++)
  {
    int score = 0;
    for(int j=i+1;j<MIN(i+50, wd);j++)
    {
      // make sure a and b are positive, reject sample otherwise
      double c  = p32[i + 0*wd];
      if(c < 50) continue;
      double m1 = p32[i + 1*wd];
      double m2 = p32[i + 2*wd];
      double x1 = m1/c - module->img_param.black[1];
      double y1 = m2/c - m1/c*m1/c;

      c  = p32[j + 0*wd];
      if(c < 50) continue;
      m1 = p32[j + 1*wd];
      m2 = p32[j + 2*wd];
      double x2 = m1/c - module->img_param.black[1];
      double y2 = m2/c - m1/c*m1/c;

      if(y1 <= 0 || y2 <= 0) continue;
      double b = (y2-y1)/(x2-x1);
      if(!(b > 0.0)) continue;
      double a  = y1 - x1 * b;
      if(!(a > 0.0)) continue;
      if(!(a < 35000.0)) continue; // half the range noise? that would be extraordinary

      if(++score > 25) // count a valid sample for this i
      {
        valid[valid_cnt++] = i;
        break; // no more j loop needed
      }
    }
  }

  // incredibly simplistic linear regression from stack overflow.
  // compute covariance matrix and from that the parameters a, b:
  // FILE *d = fopen("test.dat", "wb");
  double sx = 0.0, sx2 = 0.0, sy = 0.0, sy2 = 0.0, sxy = 0.0;
  double cnt = 0.0;
  double white = log2(module->img_param.white[1])/16.0f;
  double black = log2(module->img_param.black[1])/16.0f;
  for(int ii=0;ii<valid_cnt;ii++)
  {
    int i = valid[ii];
    double c  = p32[i + 0*wd];
    double x = exp2((i / (double)wd * (white - black) + black) * 16.0) - module->img_param.black[1];
    double m1 = p32[i + 1*wd];
    // double x = m1/c - module->img_param.black[1]; // agrees with x as above
    double m2 = p32[i + 2*wd];
    double y = m2/c - m1/c*m1/c;
    cnt += c;
    sx  += x * c;
    sx2 += x*x * c;
    sy  += y * c;
    sy2 += y*y * c;
    sxy += x*y * c;
    // fprintf(d, "%g %g\n", x, y);
  }
  // fclose(d);

  float denom = cnt * sx2 - sx*sx;
  // TODO: catch / 0 or overflows
  float b = (cnt * sxy - sx * sy) / denom;
  float a = (sy * sx2 - sx * sxy) / denom;

  char filename[512];
  snprintf(filename, sizeof(filename), "%s-%s-%d.nprof",
      module->img_param.maker,
      module->img_param.model,
      (int)module->img_param.iso);
  fprintf(stderr, "[nprof] writing '%s'\n", filename);
  FILE* f = fopen(filename, "wb");
  if(f)
  {
    fprintf(f, "%g %g\n", a, b);
    fclose(f);
  }
  free(valid);
}
