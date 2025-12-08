// pu transfer function for oidn-style data preprocessing

const float PU_Y0 =  1.57945760e-06, PU_Y1 =  3.22087631e-02;
const float PU_X0 =  2.23151711e-03, PU_X1 =  3.70974749e-01;
const float PU_A  =  1.41283765e+03,
            PU_B  =  1.64593172e+00,
            PU_C  =  4.31384981e-01,
            PU_D  = -2.94139609e-03,
            PU_E  =  1.92653254e-01,
            PU_F  =  6.26026094e-03,
            PU_G  =  9.98620152e-01;

float pu_forward(float y)
{
  if(y <= PU_Y0) return PU_A * y;
  if(y <= PU_Y1) return PU_B * pow(y, PU_C) + PU_D;
  return                PU_E * log(y+ PU_F) + PU_G;
}

const float HDR_Y_MAX = 65504.;
const float PU_NORM_SCALE = 0.31896715517604124;//1./pu_forward(HDR_Y_MAX);

float pu_transfer_function(float y)
{
  return pu_forward(y) * PU_NORM_SCALE;
}

float pu_inverse(float x)
{
  if(x <= PU_X0) return x / PU_A;
  if(x <= PU_X1) return pow((x-PU_D)/PU_B, 1./PU_C);
  return                exp((x-PU_G)/PU_E) - PU_F;
}

float pu_transfer_function_inv(float x)
{
  return pu_inverse(x / PU_NORM_SCALE);
}
