#include "../o-pfm/half.h"

#include <stdio.h>
#include <stdlib.h>
#include <zlib.h>

int main(int argc, char *argv[])
{
  float white[2] = {.3127266, .32902313}; // D65
  float endf = 0.078125;
  int wd, ht;
  float *buf;
  FILE *f = fopen(argv[1], "rb");
  if(!f)
  {
    fprintf(stderr, "usage: ./cleanlsbuf <lsbuf.pfm>\n");
    exit(1);
  }
  fscanf(f, "PF\n%d %d\n%*[^\n]", &wd, &ht);
  fgetc(f);
  buf = malloc(sizeof(float)*3*wd*ht);
  fread(buf, sizeof(float)*3, wd*ht, f);
  fclose(f);
  int end = wd * endf;
  // interpolate to white on straight line.
  // TODO: maybe this destroys identity mapping for sat=1?
  for(int j=0;j<ht;j++) for(int i=0;i<end;i++)
  {
    float t = i/(float)end;
    for(int k=0;k<2;k++)
      buf[3*(j*wd+i)+k] = white[k] * (1.0f-t) + buf[3*(j*wd+end)+k] * t;
  }
  // compactify channels:
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
    for(int k=0;k<2;k++) buf[2*(j*wd+i)+k] = buf[3*(j*wd+i)+k];
  // convert to half
  uint32_t size = 2*sizeof(uint16_t)*wd*ht;
  uint16_t *b16 = malloc(size);
  for(int k=0;k<2*wd*ht;k++) b16[k] = float_to_half(buf[k]);
  typedef struct header_t
  {
    uint32_t magic;
    uint16_t version;
    uint16_t channels;
    uint32_t wd;
    uint32_t ht;
  }
  header_t;
  header_t head = (header_t) {
    .magic    = 1234,
    .version  = 1,
    .channels = 2,
    .wd       = wd,
    .ht       = ht,
  };
  // write out uncompressed, xz does another 1.1M -> 200K
  f = fopen("clean.lut", "wb");
  if(f)
  {
    fwrite(&head, sizeof(head), 1, f);
    fwrite(b16, size, 1, f);
  }
  fclose(f);
  exit(0);
}
