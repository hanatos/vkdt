// #define OPTIMISER 1 // gauss newton
#define OPTIMISER 2 // adam
// #define OPTIMISER 3 // nelder mead

#include "core/core.h"
#include "core/solve.h"
#include "core/lut.h"
#include "cie1931.h"
#include "cc24.h"
#include "sigmoid.h"
#include "spectrum.h"
#include "matrices.h"
#include "dngproc.h"
#include "cfa.h"
#include "xrand.h"

// global configuration:
// - cfa model: plain gauss sigmoid pca
// - optimiser: gauss/newton adam nelder/mead
// - parameter: number of iterations
static int num_it = 20000;
static int cfa_model = 1;
static int cfa_num_coeff;
static double cfa_param[3*36] = {0.1}; // init to something. zero has zero derivatives and is thus bad.

static const int cfa_pca_num_coeff = 3; // does absolutely crazy things with more degrees of freedom
static const int cfa_sigmoid_num_coeff = 5; // generally becomes *worse* for more degrees of freedom
static const int cfa_gauss_num_coeff = 20; // hardcoded to that (width of gaussians, could fix i guess)
static double cfa_gauss_param[3*36] = {
  0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95, // normalised wavelength
  0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,           // height of gaussian
  0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95,
  0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
  0.05, 0.15, 0.25, 0.35, 0.45, 0.55, 0.65, 0.75, 0.85, 0.95,
  0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1,
};
// initial guess for straight estimation
static const int cfa_plain_num_coeff = 36;
static double cfa_plain_param[3*36] = { // 380..730 10nm inited from canon 5d2
  // red
  0.0354000000000, 0.0295000000000, 0.0173000000000, 0.0162000000000, 0.0181000000000,
  0.0139000000000, 0.0125000000000, 0.0107000000000, 0.0109000000000, 0.0105000000000,
  0.0126000000000, 0.0118000000000, 0.0195000000000, 0.0397000000000, 0.0912000000000,
  0.1667000000000, 0.1654000000000, 0.1487000000000, 0.1817000000000, 0.2920000000000,
  0.4343000000000, 0.4914000000000, 0.5136000000000, 0.4413000000000, 0.3824000000000,
  0.3134000000000, 0.2482000000000, 0.1757000000000, 0.1366000000000, 0.0820000000000,
  0.0365000000000, 0.0095000000000, 0.0076000000000, 0.0070000000000, 0.0070000000000,
  0.0052000000000,
  // green
  0.0359000000000, 0.0300000000000, 0.0197000000000, 0.0240000000000, 0.0495000000000,
  0.0542000000000, 0.0809000000000, 0.0902000000000, 0.1267000000000, 0.2794000000000,
  0.5298000000000, 0.6216000000000, 0.8288000000000, 0.9289000000000, 0.9776000000000,
  1.0000000000000, 0.8925000000000, 0.8624000000000, 0.7431000000000, 0.6499000000000,
  0.4963000000000, 0.3550000000000, 0.2328000000000, 0.1087000000000, 0.0599000000000,
  0.0344000000000, 0.0235000000000, 0.0146000000000, 0.0128000000000, 0.0111000000000,
  0.0075000000000, 0.0066000000000, 0.0072000000000, 0.0064000000000, 0.0073000000000,
  0.0053000000000,
  // blue
  0.0334000000000, 0.0320000000000, 0.0279000000000, 0.1461000000000, 0.5138000000000,
  0.6375000000000, 0.7291000000000, 0.8180000000000, 0.8515000000000, 0.8336000000000,
  0.7204000000000, 0.6129000000000, 0.4501000000000, 0.3204000000000, 0.1682000000000,
  0.0910000000000, 0.0524000000000, 0.0351000000000, 0.0216000000000, 0.0163000000000,
  0.0131000000000, 0.0107000000000, 0.0098000000000, 0.0079000000000, 0.0068000000000,
  0.0069000000000, 0.0075000000000, 0.0078000000000, 0.0082000000000, 0.0070000000000,
  0.0054000000000, 0.0059000000000, 0.0067000000000, 0.0064000000000, 0.0073000000000,
  0.0053000000000,
};


// reference data.
// a) via two cc24 photographs, incandescent + daylight:
static double ref_picked_a  [24][3];  // cc24 patches A-lit   reference photographed, colour picked, and loaded here, in camera rgb
static double ref_picked_d65[24][3];  // cc24 patches D65-lit reference photographed, colour picked, and loaded here, in camera rgb
// b) via an adobe dng profile containing two matrices (+luts etc):
static double ref[24][3];             // cc24 patches reference integrated against the cie observer, in XYZ
static dng_profile_t profile_a;
static dng_profile_t profile_d65;

// integrate with cfa in the loop: cc24 * (A|D65) * cfa = camera rgb
void integrate_cfa(
    double        res[24][3], // output results in camera rgb
    const double *cfa_p,      // opaque parameters to cfa subsystem
    const double *ill)        // pass cie_a or cie_d65 here
{
  for(int s=0;s<24;s++)
    res[s][0] = res[s][1] = res[s][2] = 0.0;
  for(int i=0;i<cc24_nwavelengths;i++)
  {
    for(int s=0;s<24;s++)
    {
       res[s][0] += cc24_spectra[s][i] * 
         cie_interp(ill, cc24_wavelengths[i]) *
         (cfa_red  (cfa_model, cfa_num_coeff, cfa_p+0*cfa_num_coeff, cc24_wavelengths[i]));
       res[s][1] += cc24_spectra[s][i] *
         cie_interp(ill, cc24_wavelengths[i]) *
         (cfa_green(cfa_model, cfa_num_coeff, cfa_p+1*cfa_num_coeff, cc24_wavelengths[i]));
       res[s][2] += cc24_spectra[s][i] *
         cie_interp(ill, cc24_wavelengths[i]) *
         (cfa_blue (cfa_model, cfa_num_coeff, cfa_p+2*cfa_num_coeff, cc24_wavelengths[i]));
    }
  }
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    res[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (cc24_nwavelengths-1.0);
}

// reference integration: cc24 / D50 * cie_xyz = xyz(d50)
// divide by d50 to make d50 illumination map to 1 1 1 
void integrate_ref(
    double res[24][3])
{
  for(int s=0;s<24;s++)
    res[s][0] = res[s][1] = res[s][2] = 0.0;
  for(int i=0;i<cc24_nwavelengths;i++)
  {
    for(int s=0;s<24;s++)
    {
       res[s][0] += cc24_spectra[s][i]
         / cie_interp(cie_d50, cc24_wavelengths[i]) * 7.5e-5
         // * 1e-2
         // * cie_interp(cie_d50, cc24_wavelengths[i]) * 0.9
         // * cie_interp(cie_d65, cc24_wavelengths[i])
         * cie_interp(cie_x, cc24_wavelengths[i]);
       res[s][1] += cc24_spectra[s][i]
         / cie_interp(cie_d50, cc24_wavelengths[i]) * 7.5e-5
         // * 1e-2
         // * cie_interp(cie_d50, cc24_wavelengths[i]) * 0.9
         // * cie_interp(cie_d65, cc24_wavelengths[i])
         * cie_interp(cie_y, cc24_wavelengths[i]);
       res[s][2] += cc24_spectra[s][i]
         / cie_interp(cie_d50, cc24_wavelengths[i]) * 7.5e-5
         // * 1e-2
         // * cie_interp(cie_d50, cc24_wavelengths[i]) * 0.9
         // * cie_interp(cie_d65, cc24_wavelengths[i])
         * cie_interp(cie_z, cc24_wavelengths[i]);
    }
  }
  for(int s=0;s<24;s++) for(int k=0;k<3;k++)
    res[s][k] *= (cc24_wavelengths[cc24_nwavelengths-1] - cc24_wavelengths[0]) /
      (cc24_nwavelengths-1.0);
  for(int s=0;s<24;s++) normalise1(res[s]);
}

// this loss takes differences in xyz(d50).
// use dng process with two profiles for two illuminants
// integrate cc24 * (A | D65) -> cfa -> cam rgb -> dng proc -> xyz d50
void loss(
    double *p, // parameters: 3x cfa spectrum
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points (=1)
    void   *data)
{
  double res[24][3], xyz[3];
  double err = 0.0;
  // integrate test spectra against tentative cfa and illumination spectrum:
  for(int ill=1;ill<=2;ill++)
  {
    integrate_cfa(res, p, ill==1?cie_a:cie_d65);
    // compare res to reference values (mean squared error):
    for(int i=0;i<24;i++)
    {
      dng_process(ill==1?&profile_a:&profile_d65, res[i], xyz);
      normalise1(xyz);
      err += (xyz[0] - ref[i][0])*(xyz[0] - ref[i][0])/24.0;
      err += (xyz[1] - ref[i][1])*(xyz[1] - ref[i][1])/24.0;
      err += (xyz[2] - ref[i][2])*(xyz[2] - ref[i][2])/24.0;
    }
  }
  // smoothness term:
  err += 1e-5 * cfa_smoothness(cfa_model, cfa_num_coeff, p);
  x[0] = err;
}

// this loss works on reference photographs.
// it computes camera rgb values by integrating the cc24 spectra against a tentative cfa:
// cc24 * (A | D65) -> cfa -> cam rgb
void loss_pictures(
    double *p, // parameters: 3x cfa spectrum
    double *x, // output data, write here
    int     m, // number of parameters
    int     n, // number of data points (=1)
    void   *data)
{
  double res[24][3];
  double err = 0.0;
  // integrate test spectra against tentative cfa and illumination spectrum:
  for(int ill=1;ill<=2;ill++)
  {
    double (*refi)[3] = ill==1 ? ref_picked_a : ref_picked_d65;
    integrate_cfa(res, p, ill==1?cie_a:cie_d65);
    // compare res to reference values (mean squared error):
    for(int i=0;i<24;i++)
    {
      normalise1(res[i]);
      err += (res[i][0] - refi[i][0])*(res[i][0] - refi[i][0])/24.0;
      err += (res[i][1] - refi[i][1])*(res[i][1] - refi[i][1])/24.0;
      err += (res[i][2] - refi[i][2])*(res[i][2] - refi[i][2])/24.0;
    }
  }
  // smoothness term:
  err += 1e-5 * cfa_smoothness(cfa_model, cfa_num_coeff, p);
  x[0] = err;
}

double objective(double *p, void *data)
{
  double err;
  loss(p, &err, 0, 0, 0);
  return err;
}

void loss_dif(
    double *p,   // parameters
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters
    int     n,   // number of data points (=1)
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1, X2, p2[m];
    const double h = 1e-10 + xrand()*1e-4;
    memcpy(p2, p, sizeof(p2));
    p2[j] += h;
    loss(p2, &X1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] -= h;
    loss(p2, &X2, m, n, data);
    jac[j] = (X1 - X2) / (2.0*h);
  }
}

void loss_pictures_dif(
    double *p,   // parameters
    double *jac, // output: derivative dx / dp (n x m entries, n-major, i.e. (dx[0]/dp[0], dx[0]/dp[1], ..)
    int     m,   // number of parameters
    int     n,   // number of data points (=1)
    void   *data)
{
  for(int j=0;j<m;j++)
  {
    double X1, X2, p2[m];
    const double h = 1e-10 + xrand()*1e-4;
    memcpy(p2, p, sizeof(p2));
    p2[j] += h;
    loss_pictures(p2, &X1, m, n, data);
    memcpy(p2, p, sizeof(p2));
    p2[j] -= h;
    loss_pictures(p2, &X2, m, n, data);
    jac[j] = (X1 - X2) / (2.0*h);
  }
}

int main(int argc, char *argv[])
{
  // warm up random number generator
  for(int k=0;k<10;k++) xrand();

  // =============================================
  //  parse command line
  // =============================================

  const char *model    = 0;
  const char *pick_a   = 0;
  const char *pick_d65 = 0;
  // const char *illuf = 0;
  for(int k=1;k<argc;k++)
  {
    if     (!strcmp(argv[k], "--picked"   ) && k+2 < argc) { pick_a = argv[++k]; pick_d65 = argv[++k]; }
    // else if(!strcmp(argv[k], "--illum" )  && k+1 < argc) illuf = argv[++k];
    else if(!strcmp(argv[k], "--num-it"   ) && k+1 < argc) num_it = atol(argv[++k]);
    else if(!strcmp(argv[k], "--cfa-model") && k+1 < argc) cfa_model = cfa_model_parse(argv[++k]);
    else model = argv[k];
  }
  switch(cfa_model)
  {
    case 1: cfa_num_coeff = cfa_pca_num_coeff;     break;
    case 2: cfa_num_coeff = cfa_gauss_num_coeff;   break;
    case 3: cfa_num_coeff = cfa_sigmoid_num_coeff; break;
    case 4: cfa_num_coeff = cfa_plain_num_coeff;   break;
  }

  if(!model)
  {
    fprintf(stderr, "mkssf: estimate spectral sensitivity functions of a cfa.\n");
    fprintf(stderr, "usage: mkssf <dng file>      lookup dng profile from this file\n"
                    // "             --illum <illum> parse 'illum.txt' as illuminant (else d65)\n"
                    "          --picked <a> <d65> load '<a>.txt' and '<d65>.txt' with cc24 values\n"
                    "                             to be used instead of the dng profile.\n"
                    "          --num-it <i>       use this number of iterations.\n"
                    "          --cfa-model <mod>  choose <mod> as a cfa model from:\n"
                    "                             pca, gauss, sigmoid, plain\n");

    exit(1);
  }

  // load dng profiles (matrices etc) from exif tags
  dng_profile_fill(&profile_a,   model, 1);
  dng_profile_fill(&profile_d65, model, 2);

#if 0
  // init default d65 illumination spectrum
  // for(int i=0;i<cc24_nwavelengths;i++) illum[i] = cie_interp(cie_d65, cc24_wavelengths[i]);
  if(illuf)
  {
    double illin[600][4];
    spectrum_load(illuf, illin);
    if(illin[0][0] != 380 || illin[35][0] != 730)
    { // need to resample if it's not cc24 layout (i.e. 380..730/10)
      fprintf(stderr, "TODO: implement support for this\n");
      exit(3);
    }
    for(int i=0;i<cc24_nwavelengths;i++) illum[i] = illin[i][1];
  }
#endif


  // =============================================
  //  init parameters and reference
  // =============================================

  // init initial cfa params and lower/upper bounds:
  double lb[3*cfa_num_coeff];
  double ub[3*cfa_num_coeff];
  if(cfa_model == 2 || cfa_model == 4)
    for(int k=0;k<3*cfa_num_coeff;k++) lb[k] = 0; // set to 0 for gaussians and plain
  else
    for(int k=0;k<3*cfa_num_coeff;k++) lb[k] = -1000;
  for(int k=0;k<3*cfa_num_coeff;k++) ub[k] =  1000;

  fprintf(stderr, "[vkdt-mkssf] starting optimiser on model '%s' with %d coeffs..\n", cfa_model_str(cfa_model), cfa_num_coeff);

  if(pick_a)
  { // init reference from cc24 chart photograph (needs to be camera rgb)
    FILE *f;
    f = fopen(pick_a, "rb"); // read illuminant A lit colour checker patches (incandescent)
    if(!f) { fprintf(stderr, "[vkdt-mkssf] could not open %s\n", pick_a); exit(2); }
    fscanf(f, "param:pick:01:picked:%lf:%lf:%lf",   &ref_picked_a[0][0], &ref_picked_a[0][1], &ref_picked_a[0][2]);
    for(int i=1;i<24;i++) fscanf(f, ":%lf:%lf:%lf", &ref_picked_a[i][0], &ref_picked_a[i][1], &ref_picked_a[i][2]);
    fclose(f);
    f = fopen(pick_d65, "rb"); // read illuminant D65 lit colour checker patches (daylight)
    if(!f) { fprintf(stderr, "[vkdt-mkssf] could not open %s\n", pick_d65); exit(2); }
    fscanf(f, "param:pick:01:picked:%lf:%lf:%lf",   &ref_picked_d65[0][0], &ref_picked_d65[0][1], &ref_picked_d65[0][2]);
    for(int i=1;i<24;i++) fscanf(f, ":%lf:%lf:%lf", &ref_picked_d65[i][0], &ref_picked_d65[i][1], &ref_picked_d65[i][2]);
    fclose(f);
  }

  // init ref by integrating against cie observer
  integrate_ref(ref);
  

  // =============================================
  //  optimise
  // =============================================

  if(cfa_model == 2) // gauss
    memcpy(cfa_param, cfa_gauss_param, sizeof(cfa_param));
  else if(cfa_model == 4) // plain
    memcpy(cfa_param, cfa_plain_param, sizeof(cfa_param));
  else
    for(int i=0;i<3*cfa_num_coeff;i++) cfa_param[i] = 1e-4 + 1e-3*xrand();

  double resid = 0.0;
#if OPTIMISER==1
  double target = 0.0;
  resid = dt_gauss_newton_cg(
      pick_a ? loss_pictures     : loss,
      pick_a ? loss_pictures_dif : loss_dif,
      cfa_param, &target, 3*cfa_num_coeff, 1,
      lb, ub, num_it, 0);
#elif OPTIMISER==2
  double target = 0.0;
  // if(profile_a.hsm || profile_d65.hsm) num_it = 1000000; // these require more work so it seems
  resid = dt_adam(
      pick_a ? loss_pictures     : loss,
      pick_a ? loss_pictures_dif : loss_dif,
      cfa_param, &target, 3*cfa_num_coeff, 1,
      lb, ub, num_it, 0,
      1e-8, 0.9, 0.99, .002, 0);
#elif OPTIMISER==3
  resid = dt_nelder_mead(cfa_param, 3*cfa_num_coeff, num_it, objective, 0, 0);
#endif


  // =============================================
  //  output (console + html report)
  // =============================================

  fprintf(stderr, "optimised parameters for '%s' model:\n", cfa_model_str(cfa_model));
  for(int i=0;i<3*cfa_num_coeff;i++) fprintf(stderr, "%g ", cfa_param[i]);
  fprintf(stderr, "\n");

  // dump shit into an html page
  char htmlfile[1024];
  snprintf(htmlfile, sizeof(htmlfile), "%s.html", model);
  FILE *fh = fopen(htmlfile, "wb");
  fprintf(fh, "<html><body><h1>%s</h1>\n", profile_a.model);
  fprintf(fh, "<div style='width:30%%;float:left'>\n");
  fprintf(fh, "<h2>A | CIE | D65</h2>\n");
  fprintf(fh, "<p>illuminant A + cfa + dng profile, illuminant D65 + cfa + dng profile, vs. ground truth cie observer in the middle</p>\n");
  fprintf(fh, "<table style='border-spacing:0;border-collapse:collapse'><tr>\n");
  double res[24][3];
  double rgb_a[24][3], rgb_d65[24][3], rgb_cie[24][3];
  double xyz[3];
  integrate_cfa(res, cfa_param, cie_a);
  for(int s=0;s<24;s++)
  {
    dng_process(&profile_a, res[s], xyz);
    normalise1(xyz);
    mat3_mulv(xyz_to_srgb, xyz, rgb_a[s]); // really not d50 xyz but whatever for debug
  }
  integrate_cfa(res, cfa_param, cie_d65);
  for(int s=0;s<24;s++)
  {
    dng_process(&profile_d65, res[s], xyz);
    normalise1(xyz);
    mat3_mulv(xyz_to_srgb, xyz, rgb_d65[s]);
  }
  for(int s=0;s<24;s++)
    mat3_mulv(xyz_to_srgb, ref[s], rgb_cie[s]);

  for(int s=0;s<24;s++)
  {
    fprintf(fh, "<td style='background-color:rgb(%g,%g,%g);width:33px;height:100px;padding-right:0px'> </td>",
        256.0*pow(fmax(0, rgb_a[s][0]), 0.45), 256.0*pow(fmax(0, rgb_a[s][1]), 0.45), 256.0*pow(fmax(0, rgb_a[s][2]), 0.45));
    fprintf(fh, "<td style='background-color:rgb(%g,%g,%g);width:33px;height:100px;padding-right:0px;padding-left:0px'> </td>",
        256.0*pow(fmax(0, rgb_cie[s][0]), 0.45), 256.0*pow(fmax(0, rgb_cie[s][1]), 0.45), 256.0*pow(fmax(0, rgb_cie[s][2]), 0.45));
    fprintf(fh, "<td style='background-color:rgb(%g,%g,%g);width:33px;height:100px;padding-left:0px'> </td>",
        256.0*pow(fmax(0, rgb_d65[s][0]), 0.45), 256.0*pow(fmax(0, rgb_d65[s][1]), 0.45), 256.0*pow(fmax(0, rgb_d65[s][2]), 0.45));
    if(s % 6 == 5) fprintf(fh, "</tr><tr>\n");
  }
  fprintf(fh, "</tr></table></div>\n");

  fprintf(fh, "<div style='width:30%%;float:left'>\n");
  fprintf(fh, "<h2>estimated cfa model %s, %d coeffs</h2>\n", cfa_model_str(cfa_model), cfa_num_coeff);
  fprintf(fh, "<img src='%s.png'/><p>residual loss %g</p></div>\n", model, resid);
  fprintf(fh, "</body></html>\n");
  fclose(fh);

  // output spectrum and plot it for the website report
  char datfile[1024];
  snprintf(datfile, sizeof(datfile), "%s.txt", profile_a.model);
  FILE *fd = fopen(datfile, "wb");
  for(int l=380;l<=780;l+=5)
    fprintf(fd, "%g %g %g %g\n",
        (double)l,
        cfa_red  (cfa_model, cfa_num_coeff, cfa_param+0*cfa_num_coeff, l),
        cfa_green(cfa_model, cfa_num_coeff, cfa_param+1*cfa_num_coeff, l),
        cfa_blue (cfa_model, cfa_num_coeff, cfa_param+2*cfa_num_coeff, l));
  fclose(fd);
  FILE *pd = popen("gnuplot", "w");
  fprintf(pd, "set term png\n");
  fprintf(pd, "set output '%s.png'\n", model);
  fprintf(pd, "plot '%s' u 1:2 w l lw 5 t 'red', '' u 1:3 w l lw 5 t 'green', '' u 1:4 w l lw 5 t 'blue'\n", datfile);
  pclose(pd);
}
