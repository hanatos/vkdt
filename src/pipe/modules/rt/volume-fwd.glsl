struct volume_t
{ // volume setup
  float mu_t;    // extinction coefficent: one over mean free path
  float albedo;  // scattering albedo = mu_s / mu_t
  float d;       // mean water droplet size for mie scattering phfct
};
const volume_t volume = volume_t(1.0/500000.0, 1.0, 9.0);
