#define WATER_DEPTH 16.0//8.0
// water does not really work with stock quake maps (because r_vis 1 etc)
#define WATER_MODE_OFF     0 // opaque water
#define WATER_MODE_FLAT    1 // flat but transparent with "caustics"
#define WATER_MODE_NORMALS 2 // flat but with normals and transparent
#define WATER_MODE_FULL    3 // full blown animated procedural displacement
#define WATER_MODE WATER_MODE_FULL

#define T_MAX 10000.0  // max ray tracing distance. sky is that -1

#define MCMC_KAPPA 1
#define MCMC_ML 2
#define MCMC_ADAPTATION MCMC_ML
