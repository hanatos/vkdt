// develop.glsl: grain synthesis and film density development.

shared vec3 sg_dmin, sg_inv_dmax_abs;
shared vec3 sg_cum0, sg_cum1, sg_inv_cum0, sg_inv_dcum1, sg_inv_dcum2;
shared vec3 sg_k0, sg_k1, sg_k2;
shared vec3 sg_u;
shared float sg_inv_2sq_l2e, sg_pos_scale;
shared vec2 sg_seed;
shared int sg_n_particles;
shared uint shared_grain_stream_base;


vec3 add_grain(ivec2 ipos, vec3 density)
{
  vec3 np = clamp((density + sg_dmin) * sg_inv_dmax_abs, 0.0, 1.0);
  vec3 p_dev_0 = clamp(np * sg_inv_cum0, 0.0, 1.0);
  vec3 p_dev_1 = clamp((np - sg_cum0) * sg_inv_dcum1, 0.0, 1.0);
  vec3 p_dev_2 = clamp((np - sg_cum1) * sg_inv_dcum2, 0.0, 1.0);
  vec3 var_0 = p_dev_0 * max(1.0 - p_dev_0 * sg_u, 0.0);
  vec3 var_1 = p_dev_1 * max(1.0 - p_dev_1 * sg_u, 0.0);
  vec3 var_2 = p_dev_2 * max(1.0 - p_dev_2 * sg_u, 0.0);
  vec3 var_comb = sg_k0 * var_0 + sg_k1 * var_1 + sg_k2 * var_2;
  if(all(lessThan(var_comb, vec3(1e-10)))) return density;
  vec3 std_comb = sqrt(max(var_comb, vec3(0.0)));
  vec2 pos = vec2(ipos) * sg_pos_scale + sg_seed;
  vec2 p = floor(pos);
  vec2 f = fract(pos);
  ivec2 ip = ivec2(p);
  vec3 acc = vec3(0.0);
  float wsq = 0.0;
  int n_particles = sg_n_particles;
  uint stream_base = shared_grain_stream_base;
  [[loop]] for (int y = -1; y <= 2; y++) {
    float dy_grid = float(y) - f.y;
    float dy_grid_sq = dy_grid * dy_grid;
    [[loop]] for (int x = -1; x <= 2; x++) {
      float dx_grid = float(x) - f.x;
      float dist_grid_sq = dx_grid * dx_grid + dy_grid_sq;
      float w_win = max(0.0, 1.0 - dist_grid_sq * 0.25);
      float window = w_win * w_win;
      if (window <= 0.0) continue;
      ivec2 cell = ip + ivec2(x, y);
      vec2 d_grid = vec2(dx_grid, dy_grid);
      vec3 cell_acc = vec3(0.0);
      float cell_wsq = 0.0;
      [[unroll]] for (int k = 0; k < 12; k++) {
        if (k >= n_particles) break;
        uvec3 h = pcg3d(uvec3(uvec2(cell), stream_base ^ uint(k)));
        vec2 jitter = vec2(float(h.x & 0xFFFu), float((h.x >> 12u) & 0xFFFu)) * (1.0 / 4096.0) - 0.5;
        vec3 n = vec3(float(h.y & 0xFFu), float((h.y >> 8u) & 0xFFu), float((h.y >> 16u) & 0xFFu)) * (2.0 / 255.0) - 1.0;
        vec2 d = d_grid + jitter;
        float w_part = exp2(-dot(d, d) * sg_inv_2sq_l2e);
        cell_acc += n * w_part;
        cell_wsq += w_part * w_part;
      }
      acc += cell_acc * window;
      wsq += cell_wsq * (window * window);
    }
  }
  vec3 final_noise = acc * inversesqrt(max(wsq, 1e-6)) * std_comb;
  return max(density + final_noise, sg_dmin * -1.0);
}

vec3 develop_film(vec3 log_raw, ivec2 ipos, sampler2D curvewarp)
{
  log_raw = clamp(log_raw, vec3(-4.0), vec3(4.0));
  vec3 y = log_raw;
  if (params.couplers > 0)
  {
    vec3 tcx = logexp_to_tc(log_raw);
    y = vec3(texture(curvewarp, vec2(tcx.r, 0.5)).r, texture(curvewarp, vec2(tcx.g, 0.5)).g, texture(curvewarp, vec2(tcx.b, 0.5)).b);
  }
  vec3 density_cmy = eval_film_density(y);
  density_cmy = mix(density_cmy, vec3(0.0), isnan(density_cmy));
  [[branch]] if(params.grain > 0) density_cmy = add_grain(ipos, density_cmy);
  return density_cmy;
}

void init_grain_workgroup()
{
  if (params.grain <= 0) return;
  if (int(gl_LocalInvocationIndex) == 0)
  {
    ivec2 input_size = textureSize(img_in, 0);
    ivec2 output_size = imageSize(img_out);
    float scale = float(input_size.x) / float(output_size.x);
    float lattice_um = 35000.0 / float(max(input_size.x, input_size.y));
    vec3 density_max = max(prep.film.model_dmax, vec3(0.1)), dmin = prep.grain.grain_dmin;
    vec3 amp0 = prep.film.model_amps_film[0], amp1 = prep.film.model_amps_film[1], amp2 = max(density_max - amp0 - amp1, 0.0);
    vec3 inv_dmax = 1.0 / density_max, density_max_abs = density_max + dmin;
    vec3 amp0_abs = amp0 + dmin * amp0 * inv_dmax, amp1_abs = amp1 + dmin * amp1 * inv_dmax, amp2_abs = amp2 + dmin * amp2 * inv_dmax;
    vec3 inv_dmax_abs = 1.0 / density_max_abs, cum0 = amp0_abs * inv_dmax_abs, cum1 = cum0 + amp1_abs * inv_dmax_abs;
    sg_dmin = dmin; sg_inv_dmax_abs = inv_dmax_abs; sg_cum0 = cum0; sg_cum1 = cum1;
    sg_inv_cum0 = 1.0 / max(cum0, vec3(1e-3));
    sg_inv_dcum1 = 1.0 / max(cum1 - cum0, vec3(1e-3));
    sg_inv_dcum2 = 1.0 / max(1.0 - cum1, vec3(1e-3));
    sg_seed = hash3(ivec2(global.hash, global.frame), 0x9e3779b9u).xy * 64.0;
    float lattice_um_sq = max(lattice_um * lattice_um, 1e-6), inv_pixel_area_um2 = 1.0 / lattice_um_sq;
    vec3 particle_frac_fast = min(prep.grain.grain_area_fast * inv_pixel_area_um2, vec3(1.0));
    vec3 particle_frac_mid = min(prep.grain.grain_area_mid * inv_pixel_area_um2, vec3(1.0));
    vec3 particle_frac_slow = min(prep.grain.grain_area_slow * inv_pixel_area_um2, vec3(1.0));
    sg_k0 = 3.0 * density_max_abs * particle_frac_fast * amp0_abs;
    sg_k1 = 3.0 * density_max_abs * particle_frac_mid * amp1_abs;
    sg_k2 = 3.0 * density_max_abs * particle_frac_slow * amp2_abs;
    const float blur_dye_clouds_um_sq = 4.0, global_blur_px = 0.89, min_effective_sigma_sq = 0.16;
    float sigma_sq_base_lattice = blur_dye_clouds_um_sq / lattice_um_sq + global_blur_px * global_blur_px;
    float coord_scale = max(1e-3, params.grain_size), inv_coord_scale = 1.0 / coord_scale;
    float eff_sigma_sq = max(sigma_sq_base_lattice * params.grain_size * params.grain_size * inv_coord_scale * inv_coord_scale, min_effective_sigma_sq);
    sg_u = clamp(prep.grain.grain_uniformity * pow(max(0.0, params.grain_uniformity), 0.333333), 0.0, 1.0);
    sg_inv_2sq_l2e = (0.5 / eff_sigma_sq) * 1.44269504;
    sg_pos_scale = scale * inv_coord_scale;
    sg_n_particles = int(clamp(round(2.0 * sigma_sq_base_lattice / eff_sigma_sq), 2.0, 12.0));
    shared_grain_stream_base = uint(global.hash) * 1664525u;
  }
  barrier();
}
