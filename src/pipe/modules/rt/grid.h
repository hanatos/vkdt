#include "merian-shaders/types.glsl.h"

#define MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL 0
#define MERIAN_QUAKE_GRID_TYPE_QUADRATIC 1

struct MCState {
    vec3 w_tgt;
    float sum_w;
    uint N;
    float w_cos;

    vec3 mv;
    float T;

    uint hash; // grid_idx and level
};

struct LightCacheVertex {
    uint hash; // grid_idx and level
    uint lock;
    f16vec3 irr;
    uint16_t N;
};

struct DistanceMCState {
    float sum_w;
    uint N;
    vec2 moments;
};

#ifndef __cplusplus
struct DistanceMCVertex {
    DistanceMCState states[DISTANCE_MC_VERTEX_STATE_COUNT];
};
#endif
