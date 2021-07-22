#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

static inline void
set(float *d, float s)
{
  for(int i=0;i<3;i++) d[i] = fabsf(s);
  if(s < 0.0) d[1] = d[2] = 0.0;
}

int main(int argc, char *argv[])
{
  float edge[36] = {0};
  float blur[36] = {0};
#if 1
  for(int i=0;i<36;i++) fscanf(stdin, "%g:", edge+i);
  for(int i=0;i<35;i++) fscanf(stdin, "%g:", blur+i);
#endif
  // for(int i=0;i<35;i++) edge[i] = blur[i] = 1.0f;
  // for(int i=0;i<35;i++) fprintf(stderr, "blur[%d] = %g\n", i, blur[i]);

  const int wd = 3*6;
  float *buf = calloc(sizeof(float), 3*2*wd*wd);

  for(int k=0;k<7;k++)
  {
    float sum = 0.0f;
    for(int i=0;i<5;i++) sum += edge[5*k+i];
    for(int i=0;i<5;i++) edge[5*k+i] -= sum/5.0f;
    sum = 0.0f;
    for(int i=0;i<5;i++) sum += blur[5*k+i];
    for(int i=0;i<5;i++) blur[5*k+i] /= sum;

    int x = k%3, y = k/3;
    float *c = buf + 3*(2*wd*(6*y+2) + 6*x+2);
    set(c,                         edge[5*k]);    // c
    set(c+3*(2*wd*-1 + 1), 0.8*0.4*edge[5*k+1]);  // c1
    set(c+3*(2*wd*-1 + 2), 0.2*0.4*edge[5*k+1]);
    set(c+3*(2*wd* 0 + 1), 0.8*0.6*edge[5*k+1]);
    set(c+3*(2*wd* 0 + 2), 0.2*0.6*edge[5*k+1]);
    set(c+3*(2*wd*-2 - 1), 0.4*0.2*edge[5*k+2]);  // c2
    set(c+3*(2*wd*-2 - 0), 0.6*0.2*edge[5*k+2]);
    set(c+3*(2*wd*-1 - 1), 0.4*0.8*edge[5*k+2]);
    set(c+3*(2*wd*-1 - 0), 0.6*0.8*edge[5*k+2]);
    set(c+3*(2*wd* 0 - 2), 0.2*0.6*edge[5*k+3]);  // c3
    set(c+3*(2*wd* 0 - 1), 0.8*0.6*edge[5*k+3]);
    set(c+3*(2*wd* 1 - 2), 0.2*0.4*edge[5*k+3]);
    set(c+3*(2*wd* 1 - 1), 0.8*0.4*edge[5*k+3]);
    set(c+3*(2*wd* 2 + 0), 0.6*0.2*edge[5*k+4]);  // c4
    set(c+3*(2*wd* 2 + 1), 0.4*0.2*edge[5*k+4]);
    set(c+3*(2*wd* 1 + 0), 0.6*0.8*edge[5*k+4]);
    set(c+3*(2*wd* 1 + 1), 0.4*0.8*edge[5*k+4]);
    c = buf + 3*(2*wd*(6*y+2) + wd+6*x+2);
    set(c,                         blur[5*k]);    // c
    set(c+3*(2*wd*-1 + 1), 0.8*0.4*blur[5*k+1]);  // c1
    set(c+3*(2*wd*-1 + 2), 0.2*0.4*blur[5*k+1]);
    set(c+3*(2*wd* 0 + 1), 0.8*0.6*blur[5*k+1]);
    set(c+3*(2*wd* 0 + 2), 0.2*0.6*blur[5*k+1]);
    set(c+3*(2*wd*-2 - 1), 0.4*0.2*blur[5*k+2]);  // c2
    set(c+3*(2*wd*-2 - 0), 0.6*0.2*blur[5*k+2]);
    set(c+3*(2*wd*-1 - 1), 0.4*0.8*blur[5*k+2]);
    set(c+3*(2*wd*-1 - 0), 0.6*0.8*blur[5*k+2]);
    set(c+3*(2*wd* 0 - 2), 0.2*0.6*blur[5*k+3]);  // c3
    set(c+3*(2*wd* 0 - 1), 0.8*0.6*blur[5*k+3]);
    set(c+3*(2*wd* 1 - 2), 0.2*0.4*blur[5*k+3]);
    set(c+3*(2*wd* 1 - 1), 0.8*0.4*blur[5*k+3]);
    set(c+3*(2*wd* 2 + 0), 0.6*0.2*blur[5*k+4]);  // c4
    set(c+3*(2*wd* 2 + 1), 0.4*0.2*blur[5*k+4]);
    set(c+3*(2*wd* 1 + 0), 0.6*0.8*blur[5*k+4]);
    set(c+3*(2*wd* 1 + 1), 0.4*0.8*blur[5*k+4]);
  }

  FILE *f = fopen("filters.pfm", "wb");
  if(f)
  {
    fprintf(f, "PF\n%d %d\n-1.0\n", 2*wd, wd);
    fwrite(buf, sizeof(float)*3, 2*wd*wd, f);
    fclose(f);
  }
  free(buf);
  exit(0);
}
