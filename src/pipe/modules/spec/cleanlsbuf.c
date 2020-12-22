#include "../o-pfm/half.h"
#include "../../../core/core.h"

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
    fprintf(stderr, "usage: ./cleanlsbuf <lsbuf.pfm> <spectra.lut>\n");
    exit(1);
  }
  fscanf(f, "PF\n%d %d\n%*[^\n]", &wd, &ht);
  fgetc(f);
  buf = malloc(sizeof(float)*3*wd*ht);
  fread(buf, sizeof(float)*3, wd*ht, f);
  fclose(f);

  typedef struct header_t
  {
    uint32_t magic;
    uint16_t version;
    uint16_t channels;
    uint32_t wd;
    uint32_t ht;
  }
  header_t;
  header_t header = {0};
  float *saturation;
  uint16_t *spectra16 = 0;
  f = fopen(argv[2], "rb");
  if(f)
  { // error handling from hell:
    fread(&header, sizeof(header_t), 1, f);
    spectra16 = malloc(sizeof(uint16_t)*4*header.wd*header.ht);
    fread(spectra16, sizeof(uint16_t)*4, header.wd*header.ht, f);
    fclose(f);
    // convert to float
    saturation = calloc(sizeof(float), 3*header.wd*header.ht);
    for(int k=0;k<header.wd*header.ht;k++)
      saturation[3*k] = half_to_float(spectra16[4*k+3]);
  }

  int end = wd * endf;
  // interpolate to white on straight line.
  // this destroys identity mapping for sat=1.
  for(int j=0;j<ht;j++) for(int i=0;i<end;i++)
  {
    float t = i/(float)end;
    float xy[2] = {
      white[0] * (1.0f-t) + buf[3*(j*wd+end)+0] * t,
      white[1] * (1.0f-t) + buf[3*(j*wd+end)+1] * t };
    for(int k=0;k<2;k++) buf[3*(j*wd+i)+k] = xy[k];
    if(header.channels)
    { // fix the identity roundtrip:
      // now go to this xy position in the spectra lut and write
      // our saturation value (normalised x) for round trip purposes.
      // we work under the assumption that this mesh here is way denser,
      // such that we don't leave holes in the interpolation scheme.
      // FIXME TODO: this seems to be widely ineffective, or not
      // to be addressing the core cause of the errors.
      // FIXME TODO: also we don't get close enough to the ridge.
      // we are building it for 360..830 already. not enough range?
      // inconsistency with the wavelength spectra.lut?
      float tcf[]  = {xy[0] * header.wd, (1.0-xy[1]) * header.ht};
      uint32_t tc[] = {
        (uint32_t)CLAMP(tcf[0] + 0.5f, 0, header.wd-1),
        (uint32_t)CLAMP(tcf[1] + 0.5f, 0, header.ht-1)};
      float otcf[] = {
        saturation[3*(tc[1]*header.wd + tc[0])+1],
        saturation[3*(tc[1]*header.wd + tc[0])+2]};
      float  dist = ( tcf[0]-tc[0])*( tcf[0]-tc[0])+( tcf[1]-tc[1])*( tcf[1]-tc[1]);
      float odist = (otcf[0]-tc[0])*(otcf[0]-tc[0])+(otcf[1]-tc[1])*(otcf[1]-tc[1]);
      if(dist < odist)
      {
        saturation[3*(tc[1]*header.wd + tc[0])+0] = (i+.5f)/(float)wd;
        saturation[3*(tc[1]*header.wd + tc[0])+1] = tcf[0];
        saturation[3*(tc[1]*header.wd + tc[0])+2] = tcf[1];
      }
    }
  }
  // compactify channels:
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
    for(int k=0;k<2;k++) buf[2*(j*wd+i)+k] = buf[3*(j*wd+i)+k];
  // convert to half
  uint32_t size = 2*sizeof(uint16_t)*wd*ht;
  uint16_t *b16 = malloc(size);
  for(int k=0;k<2*wd*ht;k++) b16[k] = float_to_half(buf[k]);
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
  if(spectra16)
  {
    for(int k=0;k<header.wd*header.ht;k++)
      spectra16[4*k+3] = float_to_half(saturation[3*k]);
    f = fopen("spectra.lut", "wb");
    if(f)
    {
      fwrite(&header, sizeof(header), 1, f);
      fwrite(spectra16, sizeof(uint16_t)*4*header.wd*header.ht, 1, f);
      fclose(f);
    }
  }
  exit(0);
}
