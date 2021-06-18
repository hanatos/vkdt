#include "../o-pfm/half.h"
#include <stdio.h>
#include <stdlib.h>
#include <png.h>

static inline int
read_png(
    uint8_t   **buf,
    uint32_t   *wd,
    uint32_t   *ht,
    const char *filename)
{
  FILE *f = fopen(filename, "rb");
  if(!f) return 1;

  png_byte dat[8];
  png_infop info_ptr = 0;
  png_infop info_ptr_end = 0;
  png_structp png_ptr = 0;

  size_t cnt = fread(dat, 1, 8, f);
  if(cnt != 8 || png_sig_cmp(dat, (png_size_t)0, 8)) goto error;

  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if(!png_ptr) goto error;

  info_ptr = png_create_info_struct(png_ptr);
  if(!info_ptr) goto error;

  if(setjmp(png_jmpbuf(png_ptr)))
  {
error:
    fclose(f);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    return 1;
  }

  png_init_io(png_ptr, f);
  png_set_sig_bytes(png_ptr, 8);
  png_read_info(png_ptr, info_ptr);

  int bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
  int color_type = png_get_color_type(png_ptr, info_ptr);

  if(color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png_ptr);
  if(color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
  {
    png_set_expand_gray_1_2_4_to_8(png_ptr);
    bit_depth = 8;
  }

  // grayscale => rgb
  // if(png->color_type == PNG_COLOR_TYPE_GRAY || png->color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    // png_set_gray_to_rgb(png->png_ptr);

  png_read_update_info(png_ptr, info_ptr);

  *wd = png_get_image_width (png_ptr, info_ptr);
  *ht = png_get_image_height(png_ptr, info_ptr);
  *buf = malloc(sizeof(char)*4**wd**ht);

  png_bytep *row_pointers = malloc(sizeof(png_bytep) * *ht);
  png_bytep row_pointer = (png_bytep)*buf;
  const size_t rowbytes = png_get_rowbytes(png_ptr, info_ptr);

  for(int j=0; j<*ht; j++)
    row_pointers[j] = row_pointer + (size_t)j * rowbytes;

  png_read_image(png_ptr, row_pointers);
  png_read_end(png_ptr, info_ptr_end);
  png_destroy_read_struct(&png_ptr, &info_ptr, &info_ptr_end);
  free(row_pointers);
  fclose(f);
  return 0;
}


int main(int argc, char *argv[])
{
  if(argc < 3)
  {
usage:
    fprintf(stderr, "usage: png2lut input.png output.lut\n");
    exit(1);
  }
  
  uint8_t *buf = 0;
  uint32_t wd, ht;
  if(read_png(&buf, &wd, &ht, argv[1])) goto usage;

  uint32_t size = sizeof(uint16_t)*4*wd*ht;
  uint16_t *b16 = malloc(size);
  for(int k=0;k<4*wd*ht;k++)
    b16[k] = float_to_half(buf[k]/255.0f);
  typedef struct header_t
  {
    uint32_t magic;
    uint16_t version;
    uint8_t  channels;
    uint8_t  datatype;
    uint32_t wd;
    uint32_t ht;
  }
  header_t;
  header_t head = (header_t) {
    .magic    = 1234,
    .version  = 2,
    .channels = 4,
    .datatype = 0,
    .wd       = wd,
    .ht       = ht,
  };
  FILE *f = fopen(argv[2], "wb");
  if(f)
  {
    fwrite(&head, sizeof(head), 1, f);
    fwrite(b16, size, 1, f);
    fclose(f);
  }
  exit(0);
}
