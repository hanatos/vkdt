#ifndef _MERIAN_SHADERS_HASH_H_
#define _MERIAN_SHADERS_HASH_H_

// Jarzynski et al. From Hash Functions for GPU Rendering
// Fastest to highest quality
// (1 → 1): iqint1, pcg, xxhash32, pcg3d, pcg4d
// (2 → 1): iqint3, xxhash32 multibyte2, pcg3d, pcg4d
// (3 → 1): pcg3d16, xxhash32 multibyte3, pcg3d, pcg4d
// (4 → 1): xxhash32 multibyte4, pcg4d
// (N → N ): pcg3d, pcg4d

uvec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return v;
}

uvec4 pcg4d(uvec4 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.w; v.y += v.z*v.x; v.z += v.x*v.y; v.w += v.y*v.z;
    v ^= v >> 16u;
    v.x += v.y*v.w; v.y += v.z*v.x; v.z += v.x*v.y; v.w += v.y*v.z;
    return v;
}

uint pcg3d16(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v.x += v.y*v.z;
    return v.x;
}

uint pcg4d16(uvec4 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.w; v.y += v.z*v.x; v.z += v.x*v.y; v.w += v.y*v.z;
    v.x += v.y*v.w;
    return v.x;
}

#endif
