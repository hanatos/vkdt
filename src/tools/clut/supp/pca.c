// construct pca basis from a few measured camera cfa sensitivity functions
// TODO: for non-negativity, try x=log(x) as a first step and then expand after
#include "../src/spectrum.h"
#include "svd.h"
#include <assert.h>

int main(int argc, char *argv[])
{
  // all 380..780 5nm we have:
  const char *spectrum_filename[] = {
    "../data/cie_observer",
    "../data/Arri_D21",
    "../data/Canon_EOS_5D_Mark_II",
    "../data/Canon_PowerShot_S90",
    "../data/Canon_XTi",
    "../data/Nikon_D200",
    "../data/Nikon_D5100",
    "../data/Nikon_D7000",
    "../data/Nikon_D700",
    "../data/Nikon_D70",
    "../data/Sony_ILCE-7RM2",
    "../data/Sony_ILCE-7SM2",
  };
  const int cnt = sizeof(spectrum_filename)/sizeof(spectrum_filename[0]);
  assert(cnt == 12);

  FILE *f = fopen("cfa_data.h", "wb");
  if(!f) exit(3);
  fprintf(f, "#pragma once\n");

  const char *channel[] = {"red", "green", "blue"};

  for(int c=0;c<3;c++)
  { // red green blue separately
    double X[12*81] = {0.0};
    double XtX[81*81] = {0.0};
    for(int s=0;s<cnt;s++)
    { // for all measured cfa spectra
      double cfa_spec[100][4];
      int sp_cnt = spectrum_load(spectrum_filename[s], cfa_spec);
      assert(sp_cnt == 81);
      for(int i=0;i<sp_cnt;i++)
        X[sp_cnt * s + i] = cfa_spec[i][c+1];
    }
    double avg[81] = {0.0};
    for(int i=0;i<81;i++) for(int s=0;s<cnt;s++) avg[i] += X[81*s+i]/cnt;
    for(int j=0;j<81;j++)
      for(int i=0;i<81;i++)
        for(int s=0;s<cnt;s++)
          XtX[81*j+i] += (X[81*s+i] - avg[i])*(X[81*s+j] - avg[j]);
    double S[81];
    double V[81][81];
    dsvd((double (*)[81])XtX, 81, 81, S, V);
    
#if 0 // DEBUG: now output the results
    for(int i=0;i<81;i++)
      fprintf(stdout, "%g %g %g %g %g %g\n",
          avg[i], XtX[81*i+0], XtX[81*i+1], XtX[81*i+2], XtX[81*i+8], XtX[81*i+14]);
    // singular values. looks like energy decays very quickly.
    for(int i=0;i<81;i++) fprintf(stderr, "%g ", S[i]);
    fprintf(stderr, "\n");
    exit(1);
#endif
    // write out header for parametric model: first couple of vectors + s
    const int maxd = 36;
    fprintf(f, "static const double cfa_evec_%s[%d][%d] = {{\n", channel[c], maxd, 81);
    for(int k=0;k<maxd;k++)
    {
      for(int i=0;i<81;i++)
        fprintf(f, "%g, ", XtX[81*i + k]);
      if(k < maxd-1) fprintf(f, "},{\n");
    }
    fprintf(f, "}};\n");
    fprintf(f, "static const double cfa_avg_%s[%d] = {\n", channel[c], 81);
    for(int i=0;i<81;i++)
      fprintf(f, "%g, ", avg[i]);
    fprintf(f, "};\n");
    fprintf(f, "static const double cfa_s_%s[%d] = {\n", channel[c], maxd);
    for(int k=0;k<maxd;k++)
      fprintf(f, "%g, ", S[k]);
    fprintf(f, "};\n");
  }
  fclose(f);
  exit(0);
}
