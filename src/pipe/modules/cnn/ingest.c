// ingest the files from gmic:
// gmic ./gmic_denoise_cnn.gmz k[3] unserialize repeat '$!' 'l[$>]' o '{b}'.pfm endl done

//
#include "core/half.h"
#include "core/lut.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

static inline float
tolittle(float f)
{
  uint32_t i = *(uint32_t *)&f;
  i = (i>>24) | ((i>>8) & 0xff00) | ((i<<8)&0xff0000) | (i<<24);
  return *(float *)&i;
}

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
  const int maxht = num * 16;   // allocate a bit too much
  int nic = 3, noc = 6;         // number of input and output channels
  uint16_t *buf = calloc(sizeof(uint16_t), 4*maxwd*maxht);
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
      assert(ht == 3);
      for(int i=0;i<wd;i++)
        for(int j=0;j<3;j++)
          output[4*i+j] = float_to_half(tolittle(input[wd*(ht-1-j) + i]));
      if(fi == 0) noc = 6; // blurred + appended
      else        noc = 3; // last conv layer with rgb output
      output += maxwd * 4; // both have only 3 output channels that consume weights
    }
    else
    { // these are mostly 145x16, i.e. 16*9+1 (3x3 kernel + bias)
      // re-swizzle into 145x4
      // the first time around it's 55x16
      const int str = 9; // 3x3 conv
      // const float filter[] = {0.1, 0.5, 0.1};
      noc = ht;
      assert(noc == 16);
      assert(nic*str + 1 == wd); // width of input texture includes bias
      for(int k=0;k<str;k++) // convolution kernel position
      for(int i=0;i<nic;i++) // input channel
      for(int j=0;j<noc;j++) // output channel
      {
        // const int ii = k%3, jj=k/3;
        const int oy = j/4, or = j - 4*oy;
        output[maxwd*4*oy + 4*str*i + 4*k + or]
          // = float_to_half(filter[jj]*filter[ii]);
         = float_to_half(
           tolittle(input[wd*(ht-1-j) + str*i + k]));
      }
      for(int j=0;j<(noc+3)/4;j++) // bias at end of each line
        for(int c=0;c<4;c++)
          if(4*j+c < noc)
            output[4*(maxwd*j + wd - 1) + c] = float_to_half(
                tolittle(input[wd*(ht-1-4*j-c)+wd-1]));
      output += maxwd * ((noc+3)/4)*4;
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
    .ht       = maxht,
  };
  FILE *f = fopen("cnn.lut", "wb");
  fwrite(&header, sizeof(header), 1, f);
  fwrite(buf, sizeof(uint16_t)*4, maxwd*maxht, f);
  fclose(f);
  exit(0);
}

