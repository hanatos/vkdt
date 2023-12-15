#include "modules/api.h"
#include "../core/core.h" // for MIN
#include "../core/fs.h"
#include "graph-history.h"

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
  r->scale = 1.0f;
}

void ui_callback(
    dt_module_t *module,
    dt_token_t   param)
{
  static char msg[128];
  char filename[512];
  snprintf(filename, sizeof(filename), "%s-%s-%d.nprof",
      module->img_param.maker,
      module->img_param.model,
      (int)module->img_param.iso);
  if(param == dt_token("test"))
  { // transfer new noise profile parameters to i-raw:main for a test
    FILE *f = fopen(filename, "rb");
    if(f)
    {
      float a = 0.0f, b = 0.0f;
      int num = fscanf(f, "%g %g", &a, &b);
      if(num == 2)
        for(int m=0;m<module->graph->num_modules;m++)
          if(module->graph->module[m].name == dt_token("i-raw") && module->graph->module[m].inst == dt_token("main"))
          { // TODO set with history?
            dt_module_set_param_float(module->graph->module+m, dt_token("noise a"), a);
            dt_module_set_param_float(module->graph->module+m, dt_token("noise b"), b);
            snprintf(msg, sizeof(msg), "setting noise parameters %g %g\n", a, b);
            module->graph->gui_msg = msg;
            module->graph->runflags = s_graph_run_all;
            break;
          }
      fclose(f);
    }
  }
  else if(param == dt_token("install"))
  { // install the new noise profile to ~/.config/vkdt/nprof/
    char fhome[512];
    size_t r = snprintf(fhome, sizeof(fhome), "%s/nprof/%s", dt_pipe.homedir, filename);
    if(r >= sizeof(fhome)) return;
    fs_dirname(fhome);
    fs_mkdir(fhome, 0755);
    r = snprintf(fhome, sizeof(fhome), "%s/nprof/%s", dt_pipe.homedir, filename);
    if(r >= sizeof(fhome)) return;
    fs_copy(fhome, filename);
    r = snprintf(msg, sizeof(msg), "installing noise profile to %s\n", fhome);
    if(r >= sizeof(msg)) return;
    module->graph->gui_msg = msg;
  }
  else if(param == dt_token("remove"))
  { // try and remove the noise profiling modules.
    // this assumes module names and instances as in the shipped noise profiling preset `noise-profile.pst`
    dt_module_remove_with_history(module->graph, dt_token("rawhist"), dt_token("np"));
    dt_module_remove_with_history(module->graph, dt_token("nprof"), dt_token("np"));
    dt_module_remove_with_history(module->graph, dt_token("display"), dt_token("view0"));
  }
}

// called after pipeline finished up to here.
// our input buffer will come in memory mapped.
void write_sink(
    dt_module_t *module,
    void *buf)
{
  // read back buffer, fit line to noise, output param a and param b to file with our maker model iso:
  float *p32 = buf;

  const int wd = module->connector[0].roi.wd;
  // const int ht = module->connector[0].roi.ht; == 5

  // examine pairs of samples and find out whether one position likely leads to
  // an outlier or not.
  int valid_cnt = 0;
  int *valid = malloc(wd * sizeof(int));
  float a = 0, b = 0;
  for(int f=0;f<7;f++)
  {
    double fk = 1.0 / pow(2, f);
    for(int i=0;i<wd;i++)
    {
      int score = 0;
      for(int j=i+1;j<wd*(0.8-fk);j++)
      {
        // fit: sigma^2 = y = a + b*x
        // to the data (x,y) by looking at pairs of (x,y) from the input. x is
        // the first moment of the observed raw data. this means that the noise
        // model is to be applied to uint16_t range x that have the black level
        // subtracted, but have not been rescaled to white. make
        // sure a and b are positive, reject sample otherwise
        double c  = p32[i + 0*wd];
        if(c < fk*64) continue;
        double m1 = p32[i + 1*wd];
        double m2 = p32[i + 2*wd];
        const double bk = module->img_param.black[1];
        double x1 = m1/c - bk;
        double y1 = m2/c - (m1/c)*(m1/c);

        c  = p32[j + 0*wd];
        if(c < fk*64) continue;
        m1 = p32[j + 1*wd];
        m2 = p32[j + 2*wd];
        double x2 = m1/c - bk;
        double y2 = m2/c - (m1/c)*(m1/c);

        if(y1 <= 0 || y2 <= 0) continue;
        double eb = (y2-y1)/(x2-x1);
        double ea  = y1 - x1 * eb;
        if(!(eb > 0.0)) continue;
        if(!(ea > 0.0)) continue;
        if(!(ea < 35000.0)) continue; // half the range noise? that would be extraordinary

        if(++score > fk*32) // count a valid sample for this i
        {
          // fprintf(stderr, "found valid a b %g %g\n", ea, eb);
          a = ea; // backup for valid_cnt = 1
          b = eb;
          valid[valid_cnt++] = i;
          break; // no more j loop needed
        }
      }
    }
    if(valid_cnt) break; // yay, no need to lower quality standards
    fprintf(stderr, "[nprof] WARN: reducing expectations %dx because we collected very few valid samples!\n", f+1);
  }
  
  if(valid_cnt <= 0)
    fprintf(stderr, "[nprof] ERR: could not find a single valid sample!\n");

  // incredibly simplistic linear regression from stack overflow.
  // compute covariance matrix and from that the parameters a, b:
  // FILE *d = fopen("test.dat", "wb");
  double sx = 0.0, sx2 = 0.0, sy = 0.0, sxy = 0.0;
  double cnt = 0.0;
  // double white = log2(module->img_param.white[1])/16.0f;
  // double black = log2(module->img_param.black[1])/16.0f;
  for(int ii=0;ii<valid_cnt;ii++)
  {
    int i = valid[ii];
    double c  = p32[i + 0*wd];
    // brightness from bin index:
    // double x = exp2((i / (double)wd * (white - black) + black) * 16.0) - module->img_param.black[1];
    double m1 = p32[i + 1*wd];
    double x = m1/c - module->img_param.black[1]; // brightness from mean, agrees with x as above
    double m2 = p32[i + 2*wd];
    double y = m2/c - m1/c*m1/c;
    cnt += c;
    sx  += x * c;
    sx2 += x*x * c;
    sy  += y * c;
    sxy += x*y * c;
    // fprintf(d, "%g %g\n", x, y);
  }
  // fclose(d);

  float denom = cnt * sx2 - sx*sx;
  if(fabs(denom) > 1e-10f)
  {
    b = (cnt * sxy - sx * sy) / denom;
    a = (sy * sx2 - sx * sxy) / denom;
  }
  if(valid_cnt <= 0)
  { // catch / 0 or overflows
    a = 100.0f; // just put in some random default.
    b = 1.0f;
  }

  char filename[512];
  snprintf(filename, sizeof(filename), "%s-%s-%d.nprof",
      module->img_param.maker,
      module->img_param.model,
      (int)module->img_param.iso);
  fprintf(stdout, "[nprof] writing '%s'\n", filename);
  FILE* f = fopen(filename, "wb");
  if(f)
  {
    fprintf(f, "%g %g\n", a, b);
    fclose(f);
  }
  free(valid);
}
