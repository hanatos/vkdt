// TODO:
// load lut + matrix
// run against test suite of spectra (cc24 data)
// plus extended patches (ccsg? generated spectra?)

// also run against camera rgb picked cc24 colour:
// load illum spetrum
// test:
//  cc24 spec * illum spec * cfa spec = picked cam rgb
//  (use code from mkillum)
// test:
//  cam rgb --lut--> rec2020 --wb--> rgb = cc24 ref rgb
//  (this last wb step may be the crux here)

#include "cie1931.h"
#include <stdlib.h>
#include <stdio.h>

void integrate(
    const double *cf,
    const int     cf_cnt,
    double       *res)
{
  // integrate cie_{xyz} * cie_d65 * sigmoid(poly(cf))
  res[0] = res[1] = res[2] = 0.0;
  int cnt = 0;
  for(int l=360;i<830;i+=5)
  {
    double s = sigmoid(poly(cf, l, cf_cnt));
    res[0] += s * cie_interp(cie_d65, l) * cie_interp(cie_x, l);
    res[1] += s * cie_interp(cie_d65, l) * cie_interp(cie_x, l);
    res[2] += s * cie_interp(cie_d65, l) * cie_interp(cie_x, l);
    cnt++;
  }
  res[0] *= (830.0-360.0) / (double)cnt;
  res[1] *= (830.0-360.0) / (double)cnt;
  res[2] *= (830.0-360.0) / (double)cnt;
}

int main(int argc, char *argv[])
{
  double d65_buf[500][4];
  int d65_cnt = spectrum_load("cie_d65", d65_buf);
  // 1) test integrity of spectra.lut: xyz->spec -> integrate to rgb and check cc24rgb
  // 2) test clut: integrate cfa * cc24 spec * illum -> clut -> check against cc24rgb
  // 3) photograph: pick camera rgb -> clut -> check against cc24 rgb
  // test cc24:
  for(int s=0;s<24;s++)
  {
    float xyz[3] = { cc24_xyz[3*s+0], cc24_xyz[3*s+1], cc24_xyz[3*s+2]};
    float coef[3];
    // TODO: lookup spectrum
    double cf[3] = { coef[0], coef[1], coef[2] };
    double res[3];
    integrate(cf, 3, res);
    double err = 0.0;
    for(int i=0;i<3;i++)
      err += (cc24_rec2020[3*s+i] - res[i])*(cc24_rec2020[3*s+i] - res[i]);
    fprintf(stderr, "patch %d error %g\n", s, err);
  }
  exit(0);
}
