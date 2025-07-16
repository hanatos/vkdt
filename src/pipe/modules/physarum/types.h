// shared definitions of parameter set
#define MAX_NUMBER_OF_WAVES 4
#define MAX_NUMBER_OF_RANDOM_SPAWN 8

struct PointSettings
{
  // int typeIndex; // unused
  // float defaultScalingFactor; // comes last in the data, moved

  float SensorDistance0;
  float SD_exponent;
  float SD_amplitude;

  float SensorAngle0;
  float SA_exponent;
  float SA_amplitude;

  float RotationAngle0;
  float RA_exponent;
  float RA_amplitude;

  float MoveDistance0;
  float MD_exponent;
  float MD_amplitude;

  float SensorBias1;
  float SensorBias2;

  float defaultScalingFactor;
  float pad; // pad up to 16 floats, a multiple of vec4
};

struct params_t
{
  // move arrays first for correct alignment
  float waveXarray[MAX_NUMBER_OF_WAVES];
  float waveYarray[MAX_NUMBER_OF_WAVES];
  float waveTriggerTimes[MAX_NUMBER_OF_WAVES];
  float waveSavedSigmas[MAX_NUMBER_OF_WAVES];
  float randomSpawnXarray[MAX_NUMBER_OF_RANDOM_SPAWN];
  float randomSpawnYarray[MAX_NUMBER_OF_RANDOM_SPAWN];

  // it follows 16 scalars which is vec4 aligned
  float decayFactor;
  float time;

  float actionAreaSizeSigma;

  float actionX;
  float actionY;

  float moveBiasActionX;
  float moveBiasActionY;

  float mouseXchange;
  float L2Action;

  int spawnParticles;
  float spawnFraction;
  int randomSpawnNumber;

  float pixelScaleFactor;
  float depositFactor;
  int colorModeType;
  int numberOfColorModes;

#ifndef GL_core_profile // c version
  struct PointSettings params[2];
#else
  PointSettings params[2];
#endif
};
