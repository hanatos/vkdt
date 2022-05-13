#include "cc24.h"
#include "cie1931.h"
#include "matrices.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  for(int s=0;s<24;s++)
  {
    double xyz[3] = {0.0};
    for(int i=0;i<cc24_nwavelengths;i++)
    {
      xyz[0] += cc24_spectra[s][i] * cie_interp(cie_x, cc24_wavelengths[i]) * cie_interp(cie_d65, cc24_wavelengths[i]);
      xyz[1] += cc24_spectra[s][i] * cie_interp(cie_y, cc24_wavelengths[i]) * cie_interp(cie_d65, cc24_wavelengths[i]);
      xyz[2] += cc24_spectra[s][i] * cie_interp(cie_z, cc24_wavelengths[i]) * cie_interp(cie_d65, cc24_wavelengths[i]);

    }
    for(int k=0;k<3;k++)
      xyz[k] *= (cc24_wavelengths[cc24_nwavelengths-1]-cc24_wavelengths[0]) / (cc24_nwavelengths-1.0);
    double rec2020[3] = {0.0};
    for(int i=0;i<3;i++)
      for(int k=0;k<3;k++)
        rec2020[i] += xyz_to_rec2020[i][k] * xyz[k];
    fprintf(stderr, ":%g:%g:%g", rec2020[0], rec2020[1], rec2020[2]);
    // fprintf(stderr, ":%g:%g:%g", xyz[0], xyz[1], xyz[2]);
  }
  fprintf(stderr, "\n");

#if 0
  FILE *f = fopen("cie_observer.txt", "wb");
  // for(int i=0;i<cc24_nwavelengths;i++)
  for(int i=380;i<=780;i+=5)
    fprintf(f, "%g %g %g %g\n", (double)i,
        cie_interp(cie_x, i),
        cie_interp(cie_y, i),
        cie_interp(cie_z, i));
  fclose(f);
#endif
  exit(0);
}
