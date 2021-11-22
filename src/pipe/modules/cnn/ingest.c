// ingest the files from gmic:
// gmic ./gmic_denoise_cnn.gmz k[3] unserialize repeat '$!' 'l[$>]' o '{b}'.pfm endl done

//
#include "core/half.h"
#include "core/lut.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

int main(int argc, char *argv[])
{
  const char *fn[] = {
    "BLUR.pfm",
    "C1_0.pfm",
    "C2_0.pfm",
    "C3_0.pfm",
    "C4_0.pfm",
    "C5_0.pfm",
    "C6_0.pfm",
    "C7_0.pfm",
    "C8_0.pfm",
    "C9_0.pfm",
    "C10_0.pfm",
    "C11_0.pfm",
    "C12_0.pfm",
    "C13_0.pfm",
    "C14_0.pfm",
    "C15_0.pfm",
    "C16.pfm",
  };
  const int num = 17;
  const int maxwd = 145;        // max width on input
  int ht = num * 16;            // allocate a bit too much
  int nic = 3, noc = 3;         // number of input and output channels
  uint16_t *buf = calloc(sizeof(uint16_t), 4*maxwd*ht);
  uint16_t *output = buf;
  for(int fi=0;fi<num;fi++)
  {
    FILE *f = fopen(fn[fi], "rb");
    int wd, ht;
    fscanf(f, "Pf\n%d %d\n%*[^\n]", &wd, &ht);
    assert(wd <= maxwd);
    float *input = calloc(sizeof(float), wd*ht);
    fgetc(f);
    fread(input, sizeof(float), wd*ht, f);
    fclose(f);

    if(fi == 0 || fi == num-1)
    { // BLUR or C16
      // compress 76x3 pixels to 76x1 (collapse outputs so we get matrix columns)
      // or 145x3 -> 145x1
      for(int i=0;i<wd;i++)
        for(int j=0;j<3;j++)
          output[4*i+j] = float_to_half(input[wd*j + i]);
      noc = 3;
      output += maxwd * 4;
    }
    else
    { // these are mostly 145x16, i.e. 16*9+1 (3x3 kernel + bias)
      // re-swizzle into 145x4
      const int str = 9; // 3x3 conv
      noc = ht;
      for(int k=0;k<str;k++) // convolution kernel position
      for(int i=0;i<nic;i++) // input channel
      for(int j=0;j<noc;j++) // output channel
      {
        const int oy = j/4, or = j - 4*oy;
        output[str*(4*(wd*oy+i)+or)+k] = float_to_half(input[nic*str*j + str*i + k]);
      }
      for(int j=0;j<noc/4;j++) // bias at end of each line
        for(int c=0;c<4;c++)
          output[4*(wd*j + wd - 1) + c] = float_to_half(input[wd*(4*j+c)+wd-1]);
      output += maxwd * noc;
    }
    nic = noc;
    free(input);
  }
  dt_lut_header_t header = (dt_lut_header_t) {
    .magic    = dt_lut_header_magic,
    .version  = dt_lut_header_version,
    .channels = 4,
    .datatype = dt_lut_header_f16,
    .wd       = maxwd,
    .ht       = ht,
  };
  FILE *f = fopen("cnn.lut", "wb");
  fwrite(&header, sizeof(header), 1, f);
  fwrite(buf, sizeof(uint16_t)*4, maxwd*ht, f);
  fclose(f);
  exit(0);
}

