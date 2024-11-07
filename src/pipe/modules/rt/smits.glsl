

// An RGB to Spectrum Conversion for Reﬂectances
// Brian Smits, 2000
// eats linear srgb (typical texture data) and outputs hero wavelength-style 4 samples for 4 wavelengths
vec4 rgb_to_spectral_smits(vec3 srgb, vec4 lambda)
{
  const float white[]   = { 1.0000, 1.0000, 0.9999, 0.9993, 0.9992, 0.9998, 1.0000, 1.0000, 1.0000, 1.0000, };
  const float cyan[]    = { 0.9710, 0.9426, 1.0007, 1.0007, 1.0007, 1.0007, 0.1564, 0.0000, 0.0000, 0.0000, };
  const float magenta[] = { 1.0000, 1.0000, 0.9685, 0.2229, 0.0000, 0.0458, 0.8369, 1.0000, 1.0000, 0.9959, };
  const float yellow[]  = { 0.0001, 0.0000, 0.1088, 0.6651, 1.0000, 1.0000, 0.9996, 0.9586, 0.9685, 0.9840, };
  const float red[]     = { 0.1012, 0.0515, 0.0000, 0.0000, 0.0000, 0.0000, 0.8325, 1.0149, 1.0149, 1.0149, };
  const float green[]   = { 0.0000, 0.0000, 0.0273, 0.7937, 1.0000, 0.9418, 0.1719, 0.0000, 0.0000, 0.0025, };
  const float blue[]    = { 1.0000, 1.0000, 0.8916, 0.3323, 0.0000, 0.0000, 0.0003, 0.0369, 0.0483, 0.0496, };
  const float lambda_m  = 380.0;
  const float lambda_M  = 720.0;
  uvec4 bin = clamp(uvec4(10.0*(lambda-lambda_m)/(lambda_M-lambda_m) + 0.5), uvec4(0), uvec4(9));
  vec4 ret = vec4(0);
  float w = min(srgb.r, min(srgb.g, srgb.b));
  ret += w * white[bin];
}

// scot burns says these tables work well for *within srgb*
// and turingbot says here's an analytic fit (to scott's 10nm bit spacing fitted with "mean error")
// interestingly tanh and cosine inside is surprisingly close to the moment based reconstruction.
rho_R:
0.974238*tanh((12.3964+1.97617*cos(6.86814*lambda))/((-141.502)+lambda)+asinh(asinh(pow(0.00154311*1.09295*lambda,106.109-1.83122*tan(lambda))-0.0246505)))
or:
  0.973917*tanh((12.4304+1.98473*cos(6.86817*lambda))/((-141.566)+lambda)+asinh(asinh((pow(0.00154301*1.09291*lambda,104.526-tan((-0.0645922)+lambda)/0.568957)-0.0245934)/0.996319)))


rho_G: (this seems to be surprisingly hard to fit)
tanh(lambda/(0.595835*(cosh(lambda*(log(log10(cos((-5.42359)*lambda)+1.11736*lambda))-0.0228874)-lambda)-tan(3.66925+lambda))+265.309)+0.0153729)

tanh(lambda/(0.603055*cosh(lambda*(log(log10(1.22633*cos((-5.42359)*lambda)+1.11736*lambda))-0.0228972)-lambda)+264.94)+0.0151988)


tanh(lambda/(0.603273*(cosh(lambda*(log(log10(1.2184*cos((-5.42359)*lambda)+1.11736*lambda))-0.0228972)-lambda)-tan(3.66925+lambda))+264.465)+0.0151995)

rho_B:
// (0.964157+0.0074749*cos(sqrt(lambda)))*tanh(0.0174269+pow(1.10663,482.213-lambda)-cos((-0.647554)*lambda)/173.379)

(0.963014+0.00480123*atanh(cos(0.0334188*lambda)))*tanh(0.017559+pow(1.10748,482.253-lambda)-cos((-0.647575)*lambda)/169.707)

(0.963003+0.00480235*atanh(cos(0.0334188*lambda)))*tanh(0.0178119+pow(1.10732,482.28-lambda)-cos((-0.64722)*(cos(sqrt(lambda))+lambda))/187.695)

(0.962912+0.00411279*atanh(cos(0.0334175*(tan((-0.053562)*lambda)-lambda))))*tanh(0.017563+pow(1.10723,482.222-lambda)-0.00593829*cos(0.647582*lambda))
