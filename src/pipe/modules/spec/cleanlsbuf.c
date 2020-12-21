#include <stdio.h>
#include <stdlib.h>

#include "../o-pfm/half.h"

int main(int argc, char *argv[])
{
  // TODO: load lsbuf.pfm
  // TODO: from 80px/1024 interpolate back to white at x=0
  float white[2] = {.3127266, .32902313}; // D65
  float endf = 0.78125;
  int wd, ht;
  float *buf;
  FILE *f = fopen(argv[1], "rb");
  if(!f)
  {
    fprintf(stderr, "usage: ./cleanlsbuf <lsbuf.pfm>\n");
    exit(1);
  }
  fscanf(f, "PF\n%d %d\n%*[^\n]", &wd, &ht);
  buf = malloc(sizeof(float)*3*wd*ht);
  fread(buf, sizeof(float)*3, wd*ht, f);
  fclose(f);
  int end = wd * endf;
  for(int j=0;j<ht;j++) for(int i=0;i<end;i++)
  {
    float t = i/(float)end;
    for(int k=0;k<2;k++)
      buf[3*(j*wd+i)+k] = white[k] * (1.0f-t) + buf[3*(j*wd+end)+k] * t;
  }
  // compactify channels:
  for(int j=0;j<ht;j++) for(int i=0;i<end;i++)
    for(int k=0;k<2;k++) buf[2*(j*wd+i)+k] = buf[3*(j*wd+i)+k];
  // convert to half
  uint16_t *b16 = malloc(2*sizeof(uint16_t)*wd*ht);
  for(int k=0;k<2*wd*ht;k++) b16[k] = float_to_half(buf[k]);
  // TODO: compress lz77
  // TODO: write out, use header
  exit(0);
}
