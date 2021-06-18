#include "mbs3.h"
#include "quant.h"

// coarse closed form fit:
#if 0
min(a,b)=(a<b) ? a : b
max(a,b)=(a<b) ? b : a
clamp(x,a,b)=min(max(a,x),b)
c(e, f, x) = clamp((x-e)/(f-e), 0, 1)
s(e, f, x) = c(e,f,x)*c(e,f,x)*(3.0 - 2.0*c(e,f,x))
plot 'dat' w l, s(21, 60, x)*2.24-3.14 + s(10,27,x)*0.9
// potentially want to tweak the softness of the edges a bit, i.e. multiply x by something before passing into s
#endif
static const float pXYZWarpEven[] = {-3.141592654e+00f, -3.141592654e+00f, -3.141592654e+00f, -3.141592654e+00f, -3.141591857e+00f, -3.141590597e+00f, -3.141590237e+00f, -3.141432053e+00f, -3.140119041e+00f, -3.137863071e+00f, -3.133438967e+00f, -3.123406739e+00f, -3.106095749e+00f, -3.073470612e+00f, -3.024748900e+00f, -2.963566246e+00f, -2.894461907e+00f, -2.819659701e+00f, -2.741784136e+00f, -2.660533432e+00f, -2.576526605e+00f, -2.490368187e+00f, -2.407962868e+00f, -2.334138406e+00f, -2.269339880e+00f, -2.213127747e+00f, -2.162806279e+00f, -2.114787412e+00f, -2.065873394e+00f, -2.012511127e+00f, -1.952877310e+00f, -1.886377224e+00f, -1.813129945e+00f, -1.735366957e+00f, -1.655108108e+00f, -1.573400329e+00f, -1.490781436e+00f, -1.407519056e+00f, -1.323814008e+00f, -1.239721795e+00f, -1.155352390e+00f, -1.071041833e+00f, -9.869565245e-01f, -9.030071129e-01f, -8.190615375e-01f, -7.355051008e-01f, -6.533460267e-01f, -5.738969868e-01f, -4.987252020e-01f, -4.285345153e-01f, -3.638842841e-01f, -3.049676874e-01f, -2.519255365e-01f, -2.053018669e-01f, -1.653562553e-01f, -1.314421906e-01f, -1.029987187e-01f, -7.968764403e-02f, -6.109240127e-02f, -4.655459356e-02f, -3.541922943e-02f, -2.711363958e-02f, -2.108574265e-02f, -1.671688482e-02f, -1.346866146e-02f, -1.112524547e-02f, -9.497032344e-03f, -8.356318072e-03f, -7.571825815e-03f, -6.902675935e-03f, -6.366944858e-03f, -5.918354771e-03f, -5.533441575e-03f, -5.193919971e-03f, -4.886396950e-03f, -4.601974924e-03f, -4.334090492e-03f, -4.077698212e-03f, -3.829183156e-03f, -3.585922894e-03f, -3.346285814e-03f, -3.109230512e-03f, -2.873995576e-03f, -2.640047459e-03f, -2.406989618e-03f, -2.174597631e-03f, -1.942639389e-03f, -1.711031107e-03f, -1.479623979e-03f, -1.248404899e-03f, -1.017282362e-03f, -7.861339635e-04f, -5.577696588e-04f, -3.322621191e-04f, 0.000000000e+00f};

static inline float smoothstep(float e0, float e1, float x)
{
  const float t = fminf(1.0f, fmaxf(0.0f, (x-e0)/(e1-e0)));
  return t * t * (3.0f - 2.0f * t);
}

/*! Maps wavelength to phase in a manner that is supposed to allocate longer
  parts of the phase interval for wavelengths that are perceptually more
  important. It is optimized for a Fourier series with mirroring.*/
static inline float mom_warp_lambda(float wavelength)
{
#if 1
  float x = 0.2f*(wavelength-360.0f);
  return smoothstep(21, 60, x) * (M_PI-0.9) - M_PI + smoothstep(10,27,x)*0.9;
#else
  float wavelength_bin = 0.2f*(wavelength-360.0f);
  int bin_index = (int)wavelength_bin;
  float lerp_factor = wavelength_bin - (float)bin_index;
  bin_index = (bin_index<0)?0:bin_index;
  bin_index = (bin_index>93)?93:bin_index;
  return pXYZWarpEven[bin_index] * (1.0f - lerp_factor) + pXYZWarpEven[bin_index + 1] * lerp_factor;
#endif
}

// evaluates spectrum for four wavelengths
static inline void mom_eval(
    const float *trig_mom,
    const float *wavelength,
    float *out)
{
#if 1 // regular version
  float_complex exp_mom[3], eval_poly[3];
    
  prepareReflectanceSpectrumReal3(exp_mom, eval_poly, trig_mom);

  for(int i=0;i<4;i++)
  {
    const float phase = mom_warp_lambda(wavelength[i]);
    out[i] = evaluateReflectanceSpectrum3(phase, exp_mom, eval_poly);
  }
#else // lagrange version
  // this one also clips the exponential moments implicitly, so we won't get > 1 spectra
  float lagr[3];

  // we could store these in the textures for faster evaluation.
  // however we'd run into the same quantisation issues as the sigmoid.
  prepareReflectanceSpectrumLagrangeReal3(lagr, trig_mom);

  for(int i=0;i<4;i++)
  {
    // instead of warping the phase every time (this is data access! we don't want it!)
    // we should store the warped phase on the path. even better, in fact we don't need
    // the phase but sincosf(phase) (see evaluateReflectanceSpectrumRealLagrange3). so
    // this sin and cos are what we should store on the path next to the wavelengths.
    // (there's one more sincosf() + one fast_atan() which might need speed tuning)
    const float phase = mom_warp_lambda(wavelength[i]);
    out[i] = evaluateReflectanceSpectrumRealLagrange3(phase, lagr);
  }
#endif
}

static inline void mom_eval8(
    const uint8_t *trig_mom,
    const float *wavelength,
    float *out)
{
  float tmf[3] = {
    (float)dequant_fixed(trig_mom[0], 0.0, 1.0, 8),
    (float)dequant_fixed(trig_mom[1], -1.0/M_PI, 1.0/M_PI, 8),
    (float)dequant_fixed(trig_mom[2], -1.0/M_PI, 1.0/M_PI, 8),
  };
  mom_eval(tmf, wavelength, out);
}
