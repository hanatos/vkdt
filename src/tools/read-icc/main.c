#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "../../core/mat3.h"

typedef struct icc_tag_t
{
  char name[4];
  uint32_t offset;
  uint32_t size;
}
icc_tag_t;

typedef struct XYZ_t
{
  char name[4];    // = "XYZ "
  uint8_t pad[4];  // = 0
  int32_t val[3];  // x,y,z
}
XYZ_t;

typedef struct curve_t
{
  char name[4];    // = "curv"
  uint8_t pad[4];  // = 0
  uint32_t cnt;    // 0 identity, 1 gamma, 2 line, 3 ..
  uint16_t val[1]; // and more to follow, maybe
}
curve_t;

uint32_t le32(uint32_t be)
{
  return ((be>>24)&0xff) | ((be<<8)&0xff0000) | ((be>>8)&0xff00) | ((be<<24)&0xff000000);
}

uint16_t le16(uint16_t be)
{
  return (uint16_t)((be>>8)|(be<<8));
}

int main(int argc, char* argv[])
{
  if(argc < 2)
  {
    fprintf(stderr, "usage: vkdt read-icc your-display.icc\n");
    fprintf(stderr, "writes display.profile files for colour management\n");
    fprintf(stderr, "install these by copying to ~/.config/vkdt/display.DP-1, for instance.\n");
    exit(1);
  }
  const char *tagid[] = { "rXYZ", "gXYZ", "bXYZ", "rTRC", "gTRC", "bTRC"};
#define num_tags (sizeof(tagid)/sizeof(tagid[0]))
  icc_tag_t tag[num_tags] = {0};
  XYZ_t xyz[num_tags] = {0};
  curve_t curv[num_tags] = {0};

  FILE *f = fopen(argv[1], "rb");
  if(!f)
  {
    fprintf(stderr, "could not open %s!\n", argv[1]);
    exit(2);
  }
  fseek(f, 0, SEEK_END);
  size_t bufsiz = ftell(f);
  fseek(f, 0, SEEK_SET);
  uint8_t *buf = malloc(bufsiz);
  fread(buf, 1, bufsiz, f);
  fclose(f);

  // TODO search for "mntr" string because we want monitor profiles
  // TODO early out if we found all we're looking for
  // TODO parse header length and stop after tag definitions
  for(int i=0;i<bufsiz;i++)
  {
    for(int t=0;t<num_tags;t++)
    {
      if(!memcmp(buf+i, tagid[t], 4))
      {
        memcpy(tag+t, buf+i, sizeof(icc_tag_t));
        // TODO: make sure tag.size == sizeof(XYZ_t)
        tag[t].offset = le32(tag[t].offset);
        tag[t].size   = le32(tag[t].size);
        if(!memcmp(buf+tag[t].offset, "XYZ ", 4))
        {
          memcpy(xyz+t, buf+tag[t].offset, sizeof(XYZ_t));
          for(int k=0;k<3;k++) xyz[t].val[k] = le32(xyz[t].val[k]);
        }
        else if(!memcmp(buf+tag[t].offset, "curv", 4))
        {
          memcpy(curv+t, buf+tag[t].offset, sizeof(curve_t));
          curv[t].cnt = le32(curv[t].cnt);
          if(curv[t].cnt != 1)
          {
            fprintf(stderr, "curve %.4s is not a simple gamma value, ignoring!\n", tagid[t]);
            curv[t].cnt = 1;
          }
          for(int k=0;k<curv[t].cnt;k++) curv[t].val[k] = le16(curv[t].val[k]);
        }
      }
    }
  }
  free(buf);

#if 0 // debug output
  for(int t=0;t<num_tags;t++)
  {
    if(!memcmp(xyz[t].name, "XYZ ", 4))
      fprintf(stdout, "tag %.4s %g %g %g\n", tag[t].name,
          xyz[t].val[0]/0x1.0p16,
          xyz[t].val[1]/0x1.0p16,
          xyz[t].val[2]/0x1.0p16);
    else
      fprintf(stdout, "tag %.4s %g\n", tag[t].name, curv[t].val[0]/0x1.0p8);
  }
#endif
  float gamma[] = { 
          curv[3].val[0]/0x1.0p8,
          curv[4].val[0]/0x1.0p8,
          curv[5].val[0]/0x1.0p8};

  // now get rec2020 to display matrix:
  // data extracted from ITU-R_BT2020(beta).icc available at https://www.color.org/chardata/rgb/BT2020.xalter
  // we're using the already D50 adapted rec2020 matrix (with negative primaries), because it undoes precisely
  // what the icc profile connection space (D50 XYZ) caused on the other end:
  float display_to_pcs[] = {
    xyz[0].val[0]/0x1.0p16, xyz[1].val[0]/0x1.0p16, xyz[2].val[0]/0x1.0p16,
    xyz[0].val[1]/0x1.0p16, xyz[1].val[1]/0x1.0p16, xyz[2].val[1]/0x1.0p16,
    xyz[0].val[2]/0x1.0p16, xyz[1].val[2]/0x1.0p16, xyz[2].val[2]/0x1.0p16};
  float rec2020_to_pcs[] = {
     0.67345, 0.16565, 0.12505,
     0.27902, 0.67535, 0.04561,
    -0.00194, 0.02998, 0.79689};
  float pcs_to_display[9], rec2020_to_display[9] = {0};
  mat3inv(pcs_to_display, display_to_pcs);
  for(int k = 0; k < 3; k++)
    for(int i = 0; i < 3; i++)
      for(int j = 0; j < 3; j++)
        rec2020_to_display[3*k+i] += pcs_to_display[3 * k + j] * rec2020_to_pcs[3 * j + i];

  f = fopen("display.profile", "wb");
  if(!f)
  {
    fprintf(stderr, "could not open display.profile for writing!\n");
    exit(3);
  }
  fprintf(f, "%f %f %f\n", 1.0/gamma[0], 1.0/gamma[1], 1.0/gamma[2]);
  fprintf(f, "%f %f %f\n", rec2020_to_display[0], rec2020_to_display[1], rec2020_to_display[2]);
  fprintf(f, "%f %f %f\n", rec2020_to_display[3], rec2020_to_display[4], rec2020_to_display[5]);
  fprintf(f, "%f %f %f\n", rec2020_to_display[6], rec2020_to_display[7], rec2020_to_display[8]);
  fclose(f);
  fprintf(stdout, "wrote display.profile. now copy it to ~/.config/vkdt/display.DP-1\nto apply it to your DP-1 monitor (replace DP-1 with your monitor name)\n");

  exit(0);
}
