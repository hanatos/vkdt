#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#if 0
static const uint32_t qmc_prime[] = {
  2, 3, 5, 7, 11, 13, 17, 19, 23,
29, 31, 37, 41, 43, 47, 53, 59, 61, 67,
71, 73, 79, 83, 89, 97, 101, 103, 107, 109,
113, 127, 131, 137, 139, 149, 151, 157, 163, 167,
173, 179, 181, 191, 193, 197, 199, 211, 223, 227,
229, 233, 239, 241, 251, 257, 263, 269, 271, 277,
281, 283, 293, 307, 311, 313, 317, 331, 337, 347,
349, 353, 359, 367, 373, 379, 383, 389, 397, 401,
409, 419, 421, 431, 433, 439, 443, 449, 457, 461,
463, 467, 479, 487, 491, 499, 503, 509, 521, 523,
541, 547, 557, 563, 569, 571, 577, 587, 593, 599,
601, 607, 613, 617, 619, 631, 641, 643, 647, 653,
659, 661, 673, 677, 683, 691, 701, 709, 719, 727,
733, 739, 743, 751, 757, 761, 769, 773, 787, 797,
809, 811, 821, 823, 827, 829, 839, 853, 857, 859,
863, 877, 881, 883, 887, 907, 911, 919, 929, 937,
941, 947, 953, 967, 971, 977, 983, 991, 997
};

// convert integer bits to floating point value in [0,1)
static inline float qmc_to01(uint32_t i)
{
  // use more bits:
  // return i/(float)(1ul<<32);
  // faster:
  const uint32_t v = 0x3f800000 | (i >> 9);
  return (*(float*)&v) - 1.0f;
}
#endif

// floating point radical inverse
static inline float qmc_ri(uint32_t base, uint32_t i)
{
  float digit, radical, inv = 0;
  digit = radical = 1.0f/(float)base;
  while(i)
  {
    inv += digit * (float) (i%base);
    digit *= radical;
    i /= base;
  }
  return inv;
}

int main(int argc, char *argv[])
{
  // write rec2020 test image
  srand(666);
  int w = 1024, h = 1024;
  FILE *f = fopen("checker.pfm", "wb");
  fprintf(f, "PF\n%d %d\n-1.0\n", w, h);
  for(int j=0;j<h;j++) for(int i=0;i<w;i++)
  {
#if 0
    float rgb[3];
    int ii = i/(w-1.0) * 10;
    int jj = j/(h-1.0) * 10;
    rgb[0] = (ii + drand48()*0.5)/10.0;
    rgb[1] = (jj + drand48()*0.5)/10.0;
    rgb[2] = 0.5;
#endif
    int ii = i/(float)w * 16;
    int jj = j/(float)h * 16;

    uint32_t index = jj*16 + ii;

    float rgb[] = {
      index / 256.0,
      qmc_ri(2, index),
      qmc_ri(3, index)};

    // if(ii == 15)
    {
      rgb[0] = (jj+.5)/16.0;
      rgb[1] = (jj+.5)/16.0 - 0.07 * (1.0 - (jj+.5)/16.0);
      rgb[2] = 1.0;
    }
    if(i > w/2)
    {
      rgb[0] = (0+.5)/16.0;
      rgb[1] = (0+.5)/16.0 - 0.07;
      rgb[2] = 1.0;
    }

    for(int k=0;k<3;k++) for(int r=0;r<3;r++)
      rgb[k] += 0.009*(0.5 - drand48());

    // for(int k=0;k<3;k++) rgb[k] = fminf(fmaxf(0.0f, rgb[k]), 1.0f);

    fwrite(rgb, sizeof(float), 3, f);
  }
  fclose(f);
  exit(0);
}
