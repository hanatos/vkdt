#pragma once

typedef struct macadam_t
{
  float *out;
  int res; // resolution of out is res x res
  int str; // stride of out
  int iteration;
  int num_it;
  // assume value will fall off mostly smoothly from the peak value:
  float peak_val;
  float peak[2]; // coordinate with max x+y+z during macadam pass
  float radial[180][2]; // v d, in 2 degree steps, d only needed to create the table
}
macadam_t;

static inline void
macadam_integrate_spec(
    Float lambda0,  // rising edge
    Float lambda1,  // falling edge
    Float *rgb)     // output stored here
{
  Float out[3] = { 0, 0, 0 };
  for (int i = 0; i < CIE_FINE_SAMPLES_PAD; ++i)
  {
    Float lambda = lambda_tbl[i];
    Float s = 0.0;
    if(lambda0 < lambda1)
    { // peak
      if(lambda > lambda0 && lambda <= lambda1) s = 1.0;
    }
    else
    { // dip
      if(lambda <= lambda1 || lambda > lambda0) s = 1.0;
    }
    for (int j = 0; j < 3; ++j)
      out[j] += rgb_tbl[j][i] * s;
  }
  for(int k=0;k<3;k++) rgb[k] = out[k];
}

// rasterise a few values of MacAdam's limit (will store max(0.01,0.5*limit) to search for smooth spectra)
// depends on rgb_tbl precomputed from fit-spectra.h
void macadam_work(uint32_t item, void *data)
{ // enumerate all possible box spectra in the sense of [MacAdam 1935],
  const int iw0 = item;
  macadam_t *d = data;
  for(int iw1=0;iw1<CIE_FINE_SAMPLES;iw1++)
  {
    if(iw0 == iw1) continue; // will be zero anyways
    const Float lambda0 = lambda_tbl[iw0];
    const Float lambda1 = lambda_tbl[iw1];
    Float xyz[3] = {0};
    macadam_integrate_spec(lambda0, lambda1, xyz);

    Float b = xyz[0]+xyz[1]+xyz[2];
    if(b < 1e-8) continue; // paranoid safeguard. who would design ssf like that.
    double x = xyz[0]/b;
    double y = xyz[1]/b;
    tri2quad(&x,&y);
    // rasterize into map
    const int i = x*d->res+0.5f, j = y*d->res+0.5f;
    if(i>=0 && i<d->res && j>=0 && j<d->res)
    { // compute radial bin
      if(d->iteration == 2)
      { // just accumulate it all, disregard metamerism
        d->out[d->str*(j*d->res+i) + 3] = MAX(0.01, b*0.5);
      }
      else
      { // two-pass metamer selection:
        x -= d->peak[0];
        y -= d->peak[1];
        const float angle = M_PI+atan2f(y, x);
        const int cnt = sizeof(d->radial) / sizeof(d->radial[0]);
        const int bin = (int)CLAMP(cnt*angle/(2.0*M_PI) + 0.5, 0, cnt-1);
        if(d->iteration == 1 && d->radial[bin][0] <= b)
        { // reject if radial bin value > b, i.e. our sample is in the lower brightness layer
          d->out[d->str*(j*d->res+i) + 3] = b*0.5;
        }
        else if(d->iteration == 0 && d->radial[bin][1] < x*x+y*y)
        {
          d->radial[bin][0] = b;
          d->radial[bin][1] = x*x+y*y;
        }
      }
    }
  }
}
