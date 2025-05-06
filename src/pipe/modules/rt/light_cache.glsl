#define LIGHT_CACHE_MAX_N 128s
#define LIGHT_CACHE_MIN_ALPHA .01

#ifndef MERIAN_QUAKE_LC_GRID_TYPE
#error "unknown grid type"
#else
#if MERIAN_QUAKE_LC_GRID_TYPE > 1
#error "unknown grid type"
#endif
#endif


#define lc_target_grid_width(pos) (2 * LC_GRID_TAN_ALPHA_HALF * distance(params.cam_x.xyz, pos))

#if MERIAN_QUAKE_LC_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL
    #define lc_level_for_pos(pos) uint(round(LC_GRID_STEPS_PER_UNIT_SIZE * log(max(lc_target_grid_width(pos), LC_GRID_MIN_WIDTH) / LC_GRID_MIN_WIDTH) / log(LC_GRID_POWER)))
#elif MERIAN_QUAKE_LC_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_QUADRATIC
    #define lc_level_for_pos(pos) uint(round(LC_GRID_STEPS_PER_UNIT_SIZE * pow(max(lc_target_grid_width(pos) - LC_GRID_MIN_WIDTH, 0), 1 / LC_GRID_POWER)))
#endif

#if MERIAN_QUAKE_LC_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL
    #define lc_grid_width_for_level(level) (LC_GRID_MIN_WIDTH * pow(LC_GRID_POWER, level / LC_GRID_STEPS_PER_UNIT_SIZE))
#elif MERIAN_QUAKE_LC_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_QUADRATIC
    #define lc_grid_width_for_level(level) (pow(level / LC_GRID_STEPS_PER_UNIT_SIZE, LC_GRID_POWER) + LC_GRID_MIN_WIDTH)
#endif

#define lc_grid_idx_for_level_closest(level, pos) grid_idx_closest(pos, lc_grid_width_for_level(level))

#define lc_grid_idx_for_level_interpolate(level, pos) grid_idx_interpolate(pos, lc_grid_width_for_level(level), XorShift32(rng_state))

void light_cache_get_level(out f16vec3 irr, out uint16_t N, const uint level, const vec3 pos, const vec3 normal) {
    const ivec3 grid_idx = lc_grid_idx_for_level_interpolate(level, pos);
    const uint buf_idx = hash_grid_normal_level(grid_idx, normal, level, LIGHT_CACHE_BUFFER_SIZE);
    const LightCacheVertex vtx = light_cache[buf_idx];

    if (vtx.hash == hash2_grid_level(grid_idx, level)
        && !any(isinf(vtx.irr))
        && !any(isnan(vtx.irr))) {
        irr = vtx.irr;
        N = vtx.N;
    } else {
        irr = f16vec3(0);
        N = uint16_t(0);
    }
}

f16vec3 light_cache_get(const vec3 pos, const vec3 normal) {
    const uint level = lc_level_for_pos(pos);
    f16vec3 irr; uint16_t N;
    light_cache_get_level(irr, N, level, pos, normal);
    return irr;
}

void light_cache_update(const vec3 pos, const vec3 normal, const vec3 irr) {
    const uint level = lc_level_for_pos(pos);
    const ivec3 grid_idx = lc_grid_idx_for_level_interpolate(level, pos);
    const uint buf_idx = hash_grid_normal_level(grid_idx, normal, level, LIGHT_CACHE_BUFFER_SIZE);
    
    const uint old = atomicExchange(light_cache[buf_idx].lock, params.frame);
    if (old == params.frame)
        // did not get lock
        return;

    LightCacheVertex vtx = light_cache[buf_idx];

    if (vtx.hash != hash2_grid_level(grid_idx, level)
        || any(isinf(vtx.irr))
        || any(isnan(vtx.irr))) {

        // attempt to get from coarser level
        light_cache_get_level(vtx.irr, vtx.N, level + 1, pos, normal);
        vtx.hash = hash2_grid_level(grid_idx, level);
    }

    vtx.N = min(vtx.N + 1s, uint16_t(LIGHT_CACHE_MAX_N));
    vtx.irr = f16vec3(mix(vec3(vtx.irr), irr, max(1. / vtx.N, LIGHT_CACHE_MIN_ALPHA)));

    light_cache[buf_idx] = vtx;

    light_cache[buf_idx].lock = 0;
}
