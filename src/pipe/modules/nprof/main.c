#include "modules/api.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

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
  // TODO: read back uint32_t buffer, fit line to noise, output param a and param b to file with our maker model iso!
  uint32_t *p32 = buf;

  const int wd = module->connector[0].roi.wd;
  // const int ht = module->connector[0].roi.ht; == 4

  // incredibly simplistic linear regression from stack overflow.
  // compute covariance matrix and from that the parameters a, b:

  // FILE *d = fopen("test.dat", "wb");
  double sx = 0.0, sx2 = 0.0, sy = 0.0, sy2 = 0.0, sxy = 0.0;
  double cnt = 0.0;
  double white = log2(module->img_param.white[1])/16.0f;
  double black = log2(module->img_param.black[1])/16.0f;
  for(int i=0;i<wd;i++)
  {
    double c  = p32[i + 0*wd];
    if(c > 0)
    {
      double x = exp2((i / (double)wd * (white - black) + black) * 16.0) - module->img_param.black[1];
      double m1 = p32[i + 1*wd];
      double m2 = p32[i + 2*wd];
      double y = m2/c - m1/c*m1/c;
      if(y > 0)
      {
        cnt += c;
        sx  += x * c;
        sx2 += x*x * c;
        sy  += y * c;
        sy2 += y*y * c;
        sxy += x*y * c;
        // fprintf(d, "%g %g\n", x, y);
      }
    }
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
}
