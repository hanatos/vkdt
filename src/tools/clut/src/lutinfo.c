#include "../../../core/lut.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[])
{
  FILE *f = fopen(argv[1], "rb");
  if(!f)
  {
    fprintf(stderr, "usage: lutinfo <camera model>.lut\n"
                    "print metadata information that may be present in lut file\n");
    exit(1);
  }
  dt_lut_header_t header;
  fread(&header, sizeof(header), 1, f);
  size_t sz = header.datatype == dt_lut_header_f16 ? sizeof(uint16_t) : sizeof(float);
  fprintf(stderr, "lut with %d x %d with %d channels at %zu bytes per channel\n",
      header.wd, header.ht, header.channels, sz);
  sz *= header.wd * header.ht * header.channels;
  fseek(f, sz+sizeof(header), SEEK_SET);
  while(!feof(f))
  {
    char buf[BUFSIZ];
    fscanf(f, "%[^\n]", buf);
    fgetc(f);
    // size_t s = fread(buf, BUFSIZ, 1, f);
    // fprintf(stderr, "%c\n", buf[0]);
    fprintf(stdout, "%s\n", buf);
    buf[0] = 0;
    // fwrite(buf, s, 1, stdout);
  }
  // fprintf(stdout, "\n");
  exit(0);
}
