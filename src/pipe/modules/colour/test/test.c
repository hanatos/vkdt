#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include "spectrum_common.h"
#include "rgb2spec.h"

// we are trying to reproduce the streamlines on the chromaticity chart as in
// Fig. 7 (right):
// Nonlinearities in color coding: Compensating color
// appearance for the eye’s spectral sensitivity
// Yoko Mizokami, John S. Werner, Michael A. Crognale, and Michael A. Webster
// Journal of Vision (2006), 6, 997-1007


static inline float 
macadam_spectrum(
    float dom_lambda,  // dominant wavelength
    float width,       // center width of lobe
    float slope,       // slope at point of width
    float lambda)      // wavelength to evaluate the spectrum for
{
  float min_l = dom_lambda - width/2.0f;
  float max_l = dom_lambda + width/2.0f;
  float p = 0.0f;
  if(lambda >= min_l && lambda <= max_l) p = 1.0f;
  if(slope < 0.0f) return 1.0f-p;
  return p;
}

// determine spectrum by smoothed block of macadam boxes.
// the idea is that from the purple line inwards, the assumption
// of dominant wavelength + blur doesn't hold if you just do 1-that.
// instead, the spectrum should probably have smooth flanks near zero
// in this case, too.
// not at all sure how to maintain a smooth transition near the blue-white-red
// ridges.
static inline float 
macadam_smooth_spectrum(
    float dom_lambda,  // dominant wavelength
    float width,       // center width of lobe
    float slope,       // slope at point of width
    float lambda)      // wavelength to evaluate the spectrum for
{
  float min_l = dom_lambda - width/2.0f;
  float max_l = dom_lambda + width/2.0f;
  float w = 0.5f/slope;
  if(slope < 0.0f) // magenta line?
  {
    w = -w; // width of line segment with this slope from [0,1] in y
    float tmp = min_l;
    min_l = max_l;
    max_l = tmp;
  }
  else
    w = fmaxf(1e-5f, fminf(w, width/2.0f));
  // w = 10;// XXX
  // rising edge at min_l:
  float zr = fminf(0.0f, lambda - (min_l+w));
  float p1 = expf(-0.5f*zr*zr/(w*w));
  // falling edge at max_l:
  float zf = fmaxf(0.0f, lambda - (max_l-w));
  float p2 = expf(-0.5f*zf*zf/(w*w));
  if(slope < 0.0f)
    return fmaxf(p1, p2);
  return fminf(p1, p2);
}

static inline float 
sigmoid_spectrum(
    float dom_lambda,  // dominant wavelength
    float width,       // center width of lobe
    float slope,       // slope at point of width (we take the negative)
    float lambda)      // wavelength to evaluate the spectrum for
{
#if 1
  // alg. 1 in [koenig, jung, and dachsbacher 2020]
  // const float t = (fabsf(slope) * width +
  //     sqrtf(slope*slope * width*width + 1./9.)) / (2.0f * fabsf(slope)*width);
  // const float c0 = -slope * powf(t, 3.0f/2.0f) / width;
  // const float c1 = -2.0 * c0 * dom_lambda;
  // const float c2 = c0 * dom_lambda*dom_lambda - slope * width * (5.0f * powf(t, 3.0f/2.0f) - 6.0f * sqrtf(t));
  // from alisa's jupyter notebook:
    const float s = slope;
    const float w = width;
    const float z = dom_lambda;

    const float t = (fabsf(s) * w + sqrtf(s*s*w*w + 1.0/9.0) ) / (2.0*fabsf(s)*w);
    const float sqrt_t = sqrtf(t);
    const float c0 = s * sqrt_t*sqrt_t*sqrt_t / w;
    const float c1 = -2.0 * c0 * z;
    const float c2 = c0 * z*z + s*w*sqrt_t*(5.0*t - 6.0);
#else
  // my simpler version (but forgot what the values mean)
  const float c0 = slope/width;//a;
  const float c1 = -2.0*c0*dom_lambda;
  const float c2 = slope * width + c0 * dom_lambda*dom_lambda;
#endif
  const float c[] = { c0, c1, c2 };
  return rgb2spec_eval_precise(c, lambda);
}

static inline float
gauss_spectrum(
    float dom_lambda,
    float width,
    float slope,
    float lambda)
{
  float x = lambda - dom_lambda;
  float z = x*x/(width*width+1e-10f);
  float lobe = expf(-.5*z);
  if(slope > 0)
    return fmaxf(0.0f, 1.0f - lobe);
  return lobe;
}

static inline float
trapezoid_spectrum(
    float dom_lambda,
    float width,
    float slope,
    float lambda)
{
  // pretend we always run in bump mode, invert later
  float s = fabsf(slope);
  // sanitize: if width too narrow for this slope to reach 1.0, clamp slope:
  float d = width - 1.0f/s;
  if(d < 0) s = 1.0f/width;
  // now compute output value:
  float p = 0.0f;
  float dw = 1.0f/s; // width of linear step from 0..1
  if(lambda < dom_lambda - width/2.0f - 0.5f*dw)
    p = 0.0f; // left
  else if(lambda < dom_lambda - width/2.0f + 0.5f*dw)
    p = s*(lambda - (dom_lambda - width/2.0f - 0.5f*dw)); // linear interpolation
  else if(lambda < dom_lambda + width/2.0f - 0.5f*dw)
    p = 1.0f; // center region
  else if(lambda < dom_lambda + width/2.0f + 0.5f*dw)
    p = 1.0f - s*(lambda - (dom_lambda + width/2.0f - 0.5f*dw)); // linear interpolation
  // else right out region, p stays 0.
  if(slope < 0.0f) return p;
  return 1.0f-p;
}

int main(int argc, char *argv[])
{
#if 0 // this is macadam's version:
  const int w_cnt = 30;
  for(int w=0;w<w_cnt;w++)
  { // for a few widths
    int l_cnt = 60;
    for(int l=0;l<l_cnt;l++)
    { // sample a few fixed dominant wavelengths
      const float lambda = 400.0f + 300.0f * l/(l_cnt-1.0f);
      for(int i=0;i<2;i++)
      { // blow up a bit to the left and right
        float min_l = lambda - w/(w_cnt-1.0f) * 150.0f;
        float max_l = lambda + w/(w_cnt-1.0f) * 150.0f;
        float xyz[3] = {0.0f};
        const int ll_cnt = 1000;
        for(int ll=0;ll<ll_cnt;ll++)
        { // compute XYZ
          const float lambda2 = 400.0f + 300.0f * ll/(ll_cnt-1.0f);
          float add[3] = {0.0f};
          if(( i && (lambda2 >= min_l && lambda2 <= max_l)) ||
             (!i && (lambda2 <= min_l || lambda2 >= max_l)))
            spectrum_p_to_xyz(lambda2, 1.0f, add);
          for(int k=0;k<3;k++) xyz[k] += add[k];
        }
        // plot chromaticity coordinate lines
        fprintf(stdout, "%g %g ",
            xyz[0] / (xyz[0]+xyz[1]+xyz[2]),
            xyz[1] / (xyz[0]+xyz[1]+xyz[2]));
      }
    } // end dominant wavelengths
    fprintf(stdout, "\n");
  } // end width
#endif

#if 1 // smoother spectra
  const int w_cnt = 30;
  for(int w=0;w<w_cnt;w++)
  { // for a few widths
    float width = w/(w_cnt-1.0f) * 120.0f;
    int l_cnt = 60;
    for(int l=0;l<l_cnt;l++)
    { // sample a few fixed dominant wavelengths
      const float lambda = 400.0f + 300.0f * l/(l_cnt-1.0f);
      for(int i=0;i<2;i++)
      //   int i = 1;
      { // blow up a bit to the left and right
        float slope = 0.1;//2.0f/width; // spectral colours
        // float slope = -1.0f/80.0f;//-0.001; // spectral colours
        // if(i) slope = -0.01*width;//
        // if(i) slope = -slope;//-0.1;//-0.001*width;//-slope;  // magenta line
        if(i) slope = -0.1;//- 1.0f/((1.0f - w/(w_cnt-1.0f))*400.0f); // steeper towards magenta line
        // if(i)continue; // XXX
        // else continue;
        // if(w < 0.24*w_cnt) continue; // XXX
        float xyz[3] = {0.0f};
        const int ll_cnt = 1000;
        for(int ll=0;ll<ll_cnt;ll++)
        { // compute XYZ
          const float lambda2 = 400.0f + 300.0f * ll/(ll_cnt-1.0f);
          float add[3] = {0.0f};
          const float p = sigmoid_spectrum(lambda, width, slope, lambda2);
          // const float p = macadam_smooth_spectrum(lambda, width, slope, lambda2);
          // const float p = trapezoid_spectrum(lambda, width, slope, lambda2);
          spectrum_p_to_xyz(lambda2, p, add);
          for(int k=0;k<3;k++) xyz[k] += add[k];
        }
        // plot chromaticity coordinate lines
        fprintf(stdout, "%g %g ",
            xyz[0] / (xyz[0]+xyz[1]+xyz[2]),
            xyz[1] / (xyz[0]+xyz[1]+xyz[2]));
      }
    } // end dominant wavelengths
    fprintf(stdout, "\n");
  } // end width
#endif

#if 0 // output colour ramp pfm file
  FILE *f = fopen("ramp.pfm", "wb");
  const int w_cnt = 1024;
  const int l_cnt = 60;
  const int ht = 10;
  fprintf(f, "PF\n%d %d\n-1.0\n", w_cnt, l_cnt*ht);
  float *buf = malloc(sizeof(float)*3*w_cnt);
  for(int pl=0;pl<2;pl++)
  for(int l=0;l<l_cnt;l++)
  {
    const float lambda = 400.0f + 300.0f * l/(l_cnt-1.0f);
    for(int w=0;w<w_cnt;w++)
    { // for a few widths
      float width = w/(w_cnt-1.0f) * 400.0f;
      // if(pl) width = 400.0f - width;
      // TODO: change slope in lockstep with width:
      // TODO: smaller width means larger slope
      float slope = 2.0f/width; // spectral colours
      // float slope = 1.0f/180.0f; // spectral colours
      // if(pl) slope = -slope;  // purple line
      if(pl) slope = - 1.0f/((1.0f - w/(w_cnt-1.0f))*400.0f); // steeper towards magenta line
      // if(pl) slope = -0.01*width;
      float xyz[3] = {0.0f};
      const int ll_cnt = 1000;
      for(int ll=0;ll<ll_cnt;ll++)
      { // compute XYZ
        const float lambda2 = 400.0f + 300.0f * ll/(ll_cnt-1.0f);
        float add[3] = {0.0f};
        // const float p = sigmoid_spectrum(lambda, width, slope, lambda2);
        // const float p = gauss_spectrum(lambda, width, slope, lambda2);
        // const float p = trapezoid_spectrum(lambda, width, slope, lambda2);
        // const float p = macadam_spectrum(lambda, width, slope, lambda2);
        const float p = macadam_smooth_spectrum(lambda, width, slope, lambda2);
        spectrum_p_to_xyz(lambda2, p, add);
        for(int k=0;k<3;k++) xyz[k] += add[k];
      }
      // normalise to same physical brightness (not perceived, that would be b = xyz[1])
      float b = xyz[0]+xyz[1]+xyz[2];
      // float b = xyz[1];
      for(int i=0;i<3;i++) xyz[i] /= b;
      memcpy(buf+3*w, xyz, sizeof(float)*3);
      // plot chromaticity coordinate lines
      // fprintf(stdout, "%g %g ", xyz[0], xyz[1]);
    } // end width
    for(int j=0;j<ht/2;j++)
      fwrite(buf, sizeof(float), 3*w_cnt, f);
  } // end lambdas
  fclose(f);
#endif

#if 0 // construct gamut mapping lut
  const float xyz_to_rec2020[] = {
    1.7166511880, -0.3556707838, -0.2533662814,
   -0.6666843518,  1.6164812366,  0.0157685458,
    0.0176398574, -0.0427706133,  0.9421031212,
  };
  const float xyz_to_rec709[] = {
    3.2404542, -1.5371385, -0.4985314,
   -0.9692660,  1.8760108,  0.0415560,
    0.0556434, -0.2040259,  1.0572252,
  };
  // extent of the spectral locus in our target chromaticity space.
  // this is for rec2020 r/l, b/l with l=r+g+b:
  const float box[] = {-0.35, -0.05, 1.1, 1.05};

  const int num_mips = 5;
  int res[num_mips+1];
  res[0] = 1024;
  float *mip[num_mips];
  for(int k=0;k<num_mips;k++)
  {
    mip[k] = (float *)malloc(sizeof(float)*4*res[k]*res[k]);
    memset(mip[k], 0, sizeof(float)*4*res[k]*res[k]);
    res[k+1] = res[k]/2;
  }

  // 1) construct helper xy -> wavelength and blur gaussian spectrum map:
  const int w_cnt = 512;
  for(int sc=0;sc<2;sc++)
#pragma omp parallel for default(shared)
  for(int w=0;w<w_cnt;w++)
  {
    float width = w/(w_cnt-1.0f) * 400.0f;
    const int l_cnt = 2048;
    for(int l=0;l<l_cnt;l++)
    {
      // float slope = 1.0f/90.0f;
      // if(sc) slope = -slope;
      float slope = 2.0f/width;
      if(!sc) slope = - 1.0f/((1.0f - w/(w_cnt-1.0f))*400.0f); // steeper towards magenta line
      float lambda = 400.0 + 300.0*l/(l_cnt-1.0f);

      float xyz[3] = {0.0f};
      const int ll_cnt = 1000;
      for(int ll=0;ll<ll_cnt;ll++)
      { // compute XYZ
        const float lambda2 = 400.0f + 300.0f * ll/(ll_cnt-1.0f);
        float add[3] = {0.0f};
        float p;
        // XXX how do we explicitly make spectra match at the ridges from red-white-blue?
        // XXX how about we try macadam of this width and use a gaussian of constant size to blur it?
        // if(sc) p = gauss_spectrum(lambda, width, slope, lambda2);
        // else
        // p = trapezoid_spectrum(lambda, width, slope, lambda2);
        // p = macadam_smooth_spectrum(lambda, width, slope, lambda2);
        // p = macadam_spectrum(lambda, width, slope, lambda2);
        p = sigmoid_spectrum(lambda, width, slope, lambda2);
        spectrum_p_to_xyz(lambda2, p, add);
        for(int k=0;k<3;k++) xyz[k] += add[k];
      }
      // compute location in our map:
#if 0 // xy chromaticity diagram version
      float b = xyz[0]+xyz[1]+xyz[2];
      const float x = xyz[0] / b;
      const float y = xyz[1] / b;
#else // rec2020 rb version
      float rgb[3] = {0.0f};
      for(int j=0;j<3;j++) for(int i=0;i<3;i++)
        rgb[j] += xyz_to_rec2020[3*j+i]*xyz[i];
      float b = rgb[0] + rgb[1] + rgb[2];
      const float x = (rgb[0] / b - box[0])/(box[2]-box[0]);
      const float y = (rgb[2] / b - box[1])/(box[3]-box[1]);
#endif
      // find spot in map and deposit it there
      for(int k=0;k<num_mips;k++)
      {
        const int i = x*res[k], j = y*res[k];
        if(i>=0&&i<res[k]&&j>=0&&j<res[k])
        {
          float *v = mip[k] + 4*(j*res[k]+i);
          // maybe deposit only if x,y are closer to pixel center?
          v[0] = lambda;
          v[1] = width;
          v[2] = slope;
          v[3] = y;
        }
      }
    }
  }
#if 1
  // superbasic hole filling:
  for(int j=0;j<res[0];j++)
  {
    for(int i=0;i<res[0];i++)
    {
      if(mip[0][4*(j*res[0]+i)] == 0)
      {
        int ii=i/2,jj=j/2;
        for(int k=1;k<num_mips;k++,ii/=2,jj/=2)
        {
          if(mip[k][4*(jj*res[k]+ii)] > 0)
          {
            for(int c=0;c<3;c++)
              mip[0][4*(j*res[0]+i)+c] = mip[k][4*(jj*res[k]+ii)+c];
            break;
          }
        }
      }
    }
  }
#endif
  FILE *f = fopen("gauss.pfm", "wb");
  if(f)
  {
    const int m = 0;
    fprintf(f, "PF\n%d %d\n-1.0\n", res[m], res[m]);
    for(int k=0;k<res[m]*res[m];k++)
      fwrite(mip[m]+4*k, sizeof(float)*3, 1, f);
    fclose(f);
  }
  fprintf(stderr, "wrote gauss.pfm, now on to mapped.pfm\n");

  // 2) construct gamut map lut
  // for all pixels in xy chromaticity plot
#pragma omp parallel for default(shared) collapse(2)
  for(int j=0;j<res[0];j++) for(int i=0;i<res[0];i++)
  {
    float x = i/(float)res[0];
    float y = j/(float)res[0];
    // outside spectral locus? project to spectral locus
    // inside spectral locus?
    // find dominant wavelength for this colour:
    const float lambda = mip[0][4*(j*res[0]+i)+0];
    float       width  = mip[0][4*(j*res[0]+i)+1];
    const float sc     = mip[0][4*(j*res[0]+i)+2];

    float wd_out = width, wd_in = 400.0f;
    if(sc < 0.0f) wd_in = 0.0f;
    for(int k=0;k<10;k++)
    {
      // reconstruct slope according to width
      float slope = 2.0f/width;
      if(sc < 0.0f) slope = - 1.0f/(400.0f - width); // steeper towards magenta line

      float xyz[3] = {0.0f};
      const int ll_cnt = 1000;
      for(int ll=0;ll<ll_cnt;ll++)
      { // compute XYZ
        const float lambda2 = 400.0f + 300.0f * ll/(ll_cnt-1.0f);
        float add[3] = {0.0f};
        // float p = trapezoid_spectrum(lambda, width, slope, lambda2);
        // float p = macadam_smooth_spectrum(lambda, width, slope, lambda2);
        // float p = macadam_spectrum(lambda, width, slope, lambda2);
        float p = sigmoid_spectrum(lambda, width, slope, lambda2);
        spectrum_p_to_xyz(lambda2, p, add);
        for(int k=0;k<3;k++) xyz[k] += add[k];
      }
      float b = xyz[0]+xyz[1]+xyz[2];
      x = xyz[0] / b;
      y = xyz[1] / b;
      // compute xyz -> target rgb
#if 0 // rec2020 mapping
      const float *M = xyz_to_rec2020;
#else // srgb gamut mapping
      const float *M = xyz_to_rec709;
#endif
      float rgb[3] = {0.0f};
      for(int j=0;j<3;j++) for(int i=0;i<3;i++) rgb[j] += M[3*j+i]*xyz[i];
      int outside = rgb[0] < 0 || rgb[1] < 0 || rgb[2] < 0;
      if(outside)
        wd_out = width;
      else
        wd_in = width;
      if((k == 0) && !outside)
      {
        // trivial case where we are already inside.
        // avoid instability of spectrum near white.
        const float r = i/(float)res[0] * (box[2] - box[0]) + box[0];
        const float b = j/(float)res[0] * (box[3] - box[1]) + box[1];
        mip[0][4*(j*res[0]+i)+0] = r;
        mip[0][4*(j*res[0]+i)+1] = 1.0f-r-b;
        mip[0][4*(j*res[0]+i)+2] = b;
        break;
      }
      if(k == 9) // last iteration
      {
        // in any case, write rec2020 value!
        float rgb[3] = {0.0f};
        for(int j=0;j<3;j++) for(int i=0;i<3;i++) rgb[j] += xyz_to_rec2020[3*j+i]*xyz[i];
        float rb = rgb[0] + rgb[1] + rgb[2];
        // write back colour
        mip[0][4*(j*res[0]+i)+0] = rgb[0] / rb;
        mip[0][4*(j*res[0]+i)+1] = rgb[1] / rb;
        mip[0][4*(j*res[0]+i)+2] = rgb[2] / rb;
        break;
      }
      // bisect width:
      width = (wd_out + wd_in)*.5f;
    }
  }
  f = fopen("mapped.pfm", "wb");
  if(f)
  {
    const int m = 0;
    fprintf(f, "PF\n%d %d\n-1.0\n", res[m], res[m]);
    for(int k=0;k<res[m]*res[m];k++)
      fwrite(mip[m]+4*k, sizeof(float)*3, 1, f);
    fclose(f);
  }
#endif

  // TODO: unfortunately the gaussian blur fits what i would consider a
  // reasonable gradient the best. this means we'll need an inverse lut
  // that takes us from normalised xy (X+Y+Z=1) to gaussian dom l + width.
  // TODO: find out what's reasonable to do outside of the spectral locus.
  // we'll not be able to find any spectra for these, probably project
  // towards white and hope for the best :(

  // TODO: input an rgb colour, way out of gamut if need be
  // TODO: compute mac adam spectrum
  // TODO: make wider by growing the peak
  // TODO: compute tristimulus
  // TODO: plot gradients/plot chromaticity coordinates
  exit(0);
}
