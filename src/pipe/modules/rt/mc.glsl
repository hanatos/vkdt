// Configure ML
#define ML_MAX_N 1024
#define ML_MIN_ALPHA .01

#ifndef MERIAN_QUAKE_ADAPTIVE_GRID_TYPE
#error "unknown grid type"
#else
#if MERIAN_QUAKE_ADAPTIVE_GRID_TYPE > 1
#error "unknown grid type"
#endif
#endif

// GENERAL

#define mc_state_new() MCState(vec3(0.0), 0.0, 0, 0.0, vec3(0), 0.0, 0);

// return normalized direction (from pos)
#define mc_state_dir(mc_state, pos) normalize((mc_state.sum_w > 0.0 ? mc_state.w_tgt / mc_state.sum_w : mc_state.w_tgt) - pos)

#define mc_state_pos(mc_state) (mc_state.sum_w > 0.0 ? mc_state.w_tgt / mc_state.sum_w : mc_state.w_tgt)

#define mc_state_prior(mc_state, pos) (max(0.0001, DIR_GUIDE_PRIOR / merian_square(distance((pos), mc_state_pos(mc_state)))))

#define mc_state_mean_cos(mc_state, pos) ((mc_state.N * mc_state.N * clamp(mc_state.w_cos / mc_state.sum_w, 0.0, 0.9999999)) / (mc_state.N * mc_state.N + mc_state_prior(mc_state, pos)))

bool mc_light_missing(const MCState mc_state, const float mc_f, const vec3 wo, const vec3 pos) {

    if (mc_f > 0.5 * mc_state.sum_w) {
        return false;
    }

    if (params.cl_time == mc_state.T) {
        return false;
    }

    const float cos = dot(wo, mc_state_dir(mc_state, pos));

    if (cos < 0.9 + 0.1 * mc_state_mean_cos(mc_state, pos)) {
        // light might still be there 
        return false;
    }

    return true;
}

float mc_state_kappa(const MCState mc_state, const vec3 pos) {
    const float r = mc_state_mean_cos(mc_state, pos);
    return (3.0 * r - r * r * r) / (1.0 - r * r);
}

// returns the vmf lobe vec4(direction, kappa) for a position
#define mc_state_get_vmf(mc_state, pos) vec4(mc_state_dir(mc_state, pos), mc_state_kappa(mc_state, pos))

void mc_state_reweight(inout MCState mc_state, const float factor) {
    mc_state.sum_w *= factor;
    mc_state.w_tgt *= factor;
    mc_state.w_cos *= factor;
}

// add sample to lobe via maximum likelihood estimator and exponentially weighted average
void mc_state_add_sample(inout MCState mc_state,
                         const vec3 pos,         // position where the ray started
                         const float w,          // goodness
                         const vec3 target, const vec3 target_mv) {    // ray hit point

    mc_state.N = min(mc_state.N + 1, ML_MAX_N);
    const float alpha = max(1.0 / mc_state.N, ML_MIN_ALPHA);

    mc_state.sum_w = mix(mc_state.sum_w, w,          alpha);
    mc_state.w_tgt = mix(mc_state.w_tgt, w * target, alpha);
    mc_state.w_cos = min(mix(mc_state.w_cos, w * max(0, dot(normalize(target - pos), mc_state_dir(mc_state, pos))), alpha), mc_state.sum_w);
    //mc_state.w_cos = min(length(mix(mc_state.w_cos * mc_state_dir(mc_state, pos), w * normalize(target - pos), alpha)), mc_state.sum_w);

    mc_state.mv = target_mv;
    mc_state.T = params.cl_time;
}

#define mc_state_valid(mc_state) (mc_state.sum_w > 0.0)

// ADAPTIVE GRID

#define mc_target_grid_width(pos) (2 * MC_ADAPTIVE_GRID_TAN_ALPHA_HALF * distance(params.cam_x.xyz, pos))

#if MERIAN_QUAKE_ADAPTIVE_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL
    #define mc_adaptive_target_level_for_pos(pos) uint(round(MC_ADAPTIVE_GRID_STEPS_PER_UNIT_SIZE * log(max(mc_target_grid_width(pos), MC_ADAPTIVE_GRID_MIN_WIDTH) / MC_ADAPTIVE_GRID_MIN_WIDTH) / log(MC_ADAPTIVE_GRID_POWER)))
#elif MERIAN_QUAKE_ADAPTIVE_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_QUADRATIC
    #define mc_adaptive_target_level_for_pos(pos) uint(round(MC_ADAPTIVE_GRID_STEPS_PER_UNIT_SIZE * pow(max(mc_target_grid_width(pos) - MC_ADAPTIVE_GRID_MIN_WIDTH, 0), 1 / MC_ADAPTIVE_GRID_POWER)))
#endif

#define mc_adaptive_level_for_pos(pos, random) (mc_adaptive_target_level_for_pos(pos) + uint((-log2(1.0 - random))))

#if MERIAN_QUAKE_ADAPTIVE_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_EXPONENTIAL
    #define mc_grid_width_for_level(level) (MC_ADAPTIVE_GRID_MIN_WIDTH * pow(MC_ADAPTIVE_GRID_POWER, level / MC_ADAPTIVE_GRID_STEPS_PER_UNIT_SIZE))
#elif MERIAN_QUAKE_ADAPTIVE_GRID_TYPE == MERIAN_QUAKE_GRID_TYPE_QUADRATIC
    #define mc_grid_width_for_level(level) (pow(level / MC_ADAPTIVE_GRID_STEPS_PER_UNIT_SIZE, MC_ADAPTIVE_GRID_POWER) + MC_ADAPTIVE_GRID_MIN_WIDTH)
#endif

#define mc_adpative_grid_idx_for_level_closest(level, pos) grid_idx_closest(pos, mc_grid_width_for_level(level))

#define mc_adaptive_grid_idx_for_level_interpolate(level, pos) grid_idx_interpolate(pos, mc_grid_width_for_level(level), XorShift32(rng_state))

// returns (buffer_index, hash)
void mc_adaptive_buffer_index(const vec3 pos, const vec3 normal, out uint buffer_index, out uint hash) {
    const uint level = mc_adaptive_level_for_pos(pos, XorShift32(rng_state));
    const ivec3 grid_idx = mc_adaptive_grid_idx_for_level_interpolate(level, pos);
    buffer_index = hash_grid_normal_level(grid_idx, normal, level, MC_ADAPTIVE_BUFFER_SIZE);
    hash = hash2_grid_level(grid_idx, level);
}

void mc_adaptive_finalize_load(inout MCState mc_state, const uint hash) {
    mc_state.sum_w *= float(hash == mc_state.hash);
    mc_state.w_tgt += mc_state.sum_w * (params.cl_time - mc_state.T) * mc_state.mv;
}

void mc_adaptive_load(out MCState mc_state, const vec3 pos, const vec3 normal) {
    uint buffer_index, hash;
    mc_adaptive_buffer_index(pos, normal, buffer_index, hash);
    mc_state = mc_states[buffer_index];
    mc_adaptive_finalize_load(mc_state, hash);
}

void mc_adaptive_save(in MCState mc_state, const vec3 pos, const vec3 normal) {
    uint buffer_index, hash;
    mc_adaptive_buffer_index(pos, normal, buffer_index, hash);

    mc_state.hash = hash;
    mc_states[buffer_index] = mc_state;
}


// STATIC GRID

// returns (buffer_index, hash)
void mc_static_buffer_index(const vec3 pos, out uint buffer_index, out uint hash) {
    const ivec3 grid_idx = grid_idx_interpolate(pos, MC_STATIC_GRID_WIDTH, XorShift32(rng_state));
    buffer_index = hash_grid(grid_idx, MC_STATIC_BUFFER_SIZE) + MC_ADAPTIVE_BUFFER_SIZE;
    hash = hash2_grid(grid_idx);
}

void mc_static_finalize_load(inout MCState mc_state, const uint hash) {
    mc_state.sum_w *= float(hash == mc_state.hash);
    mc_state.w_tgt += mc_state.sum_w * (params.cl_time - mc_state.T) * mc_state.mv;
}

void mc_static_finalize_load(inout MCState mc_state, const uint hash, const vec3 pos, const vec3 normal) {
    mc_state.sum_w *= float(hash == mc_state.hash);
    mc_state.sum_w *= float(dot(normal, mc_state_dir(mc_state, pos)) > 0.);
    mc_state.w_tgt += mc_state.sum_w * (params.cl_time - mc_state.T) * mc_state.mv;
}

void mc_static_load(out MCState mc_state, const vec3 pos) {
    uint buffer_index, hash;
    mc_static_buffer_index(pos, buffer_index, hash);
    mc_state = mc_states[buffer_index];
    mc_static_finalize_load(mc_state, hash);
}

void mc_static_load(out MCState mc_state, const vec3 pos, const vec3 normal) {
    uint buffer_index, hash;
    mc_static_buffer_index(pos, buffer_index, hash);
    mc_state = mc_states[buffer_index];
    mc_static_finalize_load(mc_state, hash, pos, normal);
}

void mc_static_save(in MCState mc_state, const vec3 pos, const vec3 normal) {
    uint buffer_index, hash;
    mc_static_buffer_index(pos, buffer_index, hash);

    mc_state.hash = hash;
    mc_states[buffer_index] = mc_state;
}
