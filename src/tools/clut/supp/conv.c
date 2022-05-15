#include "dngproc.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    fprintf(stderr, "converts a bunch of camera rgb pixels to xyz\n"
                    "using the dng profile for debugging purposes\n");
    fprintf(stderr, "usage: conv camera.dng camrgb.pfm\n");
    exit(0);
  }

  dng_profile_t p;
  dng_profile_fill(&p, argv[1], 2); // d65 illuminant

  FILE *f = fopen(argv[2], "rb");
  FILE *g = fopen("conv.pfm", "wb");
  int wd, ht;
  fscanf(f, "PF\n%d %d\n-1.0%*[^\n]", &wd, &ht);
  fgetc(f);
  fprintf(g, "PF\n%d %d\n-1.0\n", wd, ht);

  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    float cam_rgb[3], xyz[3];
    double cam_rgbd[3], xyzd[3];
    fread(cam_rgb, sizeof(float), 3, f);
    for(int k=0;k<3;k++) cam_rgbd[k] = cam_rgb[k];
    dng_process(&p, cam_rgbd, xyzd);
    for(int k=0;k<3;k++) xyz[k] = xyzd[k];
    fwrite(xyz, sizeof(float), 3, g);
  }
  fclose(f);
  fclose(g);
}
