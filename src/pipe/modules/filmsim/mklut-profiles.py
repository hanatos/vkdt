import json
import os
import re
import tomllib
from pathlib import Path
from dotmap import DotMap
import numpy as np
import scipy.stats
import struct

# Baked spektrafilm coupler and grain presets.
_DATA_DIR = os.environ.get('SPEKTRAFILM_DATA')
_PRESETS_DIR  = Path(_DATA_DIR) / 'presets'  if _DATA_DIR else Path('../presets')
_PROFILES_DIR = Path(_DATA_DIR) / 'profiles' if _DATA_DIR else Path('.')

def _load_toml(name):
  with open(_PRESETS_DIR / name, 'rb') as fh:
    return tomllib.load(fh)

_couplers_toml = _load_toml('couplers.toml')
_grain_toml = _load_toml('grain.toml')

# spektrafilm schema defaults.
_COUPLER_SCHEMA_DEFAULTS = {
    'gamma_samelayer_rgb':     (0.341, 0.324, 0.273),
    'gamma_interlayer_r_to_gb':(0.355, 0.305),
    'gamma_interlayer_g_to_rb':(0.154, 0.358),
    'gamma_interlayer_b_to_rg':(0.171, 0.225),
    'inhibition_samelayer':    1.0,
    'inhibition_interlayer':   1.0,
}
_GRAIN_SCHEMA_DEFAULTS = {
    'rms_granularity':         (6.0, 8.0, 10.0),
    'density_min':             (0.03, 0.03, 0.03),
    'uniformity':               (0.97, 0.97, 0.97),
    'particle_scale_sublayers':(1.0, 0.5, 0.25),
}
_RMS_APERTURE_AREA_UM2 = float(np.pi * (48.0 / 2.0) ** 2)

# Reference illuminant CCTs.
_REF_ILLUMINANT_CCT = {
    'D55': 5503.0,
    'D65': 6504.0,
    'T':   2812.0,
    'TH-KG3': 3400.0,
    'K75P':   6344.0,
}

# rec2020's native white, matching filmsim.glsl's d65_xyz literal.
_D65_XYZ = np.array([0.950430, 1.0, 1.088801])

# CIE daylight basis S0/S1/S2 on the shader's 36 exposure bands (380-730nm,
# 10nm step), matching data.glsl's cie_d_s0/s1/s2 tables exactly.
_LAM36 = 380.0 + 10.0 * np.arange(36)
_CIE_D_S0_36 = np.array([
     63.4,  65.8,  94.8, 104.8, 105.9,  96.8, 113.9, 125.6, 125.5, 121.3,
    121.3, 113.5, 113.1, 110.8, 106.5, 108.8, 105.3, 104.4, 100.0,  96.0,
     95.1,  89.1,  90.5,  90.3,  88.4,  84.0,  85.1,  81.9,  82.6,  84.9,
     81.3,  71.9,  74.3,  76.4,  63.3,  71.7])
_CIE_D_S1_36 = np.array([
     38.5,  35.0,  43.4,  46.3,  43.9,  37.1,  36.7,  35.9,  32.6,  27.9,
     24.3,  20.1,  16.2,  13.2,   8.6,   6.1,   4.2,   1.9,   0.0,  -1.6,
     -3.5,  -3.5,  -5.8,  -7.2,  -8.6,  -9.5, -10.9, -10.7, -12.0, -14.0,
    -13.6, -12.0, -13.3, -12.9, -10.6, -11.6])
_CIE_D_S2_36 = np.array([
      3.0,   1.2,  -1.1,  -0.5,  -0.7,  -1.2,  -2.6,  -2.9,  -2.8,  -2.6,
     -2.6,  -1.8,  -1.5,  -1.3,  -1.2,  -1.0,  -0.5,  -0.3,   0.0,   0.2,
      0.5,   2.1,   3.2,   4.1,   4.7,   5.1,   6.7,   7.3,   8.6,   9.8,
     10.2,   8.3,   9.6,   8.5,   7.0,   7.6])
_ILLUMINANT_BB_MAX_CCT = 4000.0  # below this, use blackbody; matches the shader.

def _colour_blackbody(lam, T):
  h2, h, c, k = 6.62606957e11, 6.62606957e-34, 299792458.0, 1.3807e-23
  lambda_m = lam * 1e-9
  c1 = 2.0 * h2 * c * c / lam**5
  c2 = h * c / (lambda_m * T * k)
  return 1e-14 * c1 / (np.exp(c2) - 1.0)

def _daylight_locus_xy(T):
  T = np.clip(T, 4000.0, 25000.0)
  invT = 1.0 / T
  x = (99.11*invT + 2.9678e6*invT**2 - 4.6070e9*invT**3 + 0.244063) if T <= 7000.0 else \
      (247.48*invT + 1.9018e6*invT**2 - 2.0064e9*invT**3 + 0.237040)
  y = -3.0*x*x + 2.870*x - 0.275
  return x, y

def _daylight_weights(T):
  x, y = _daylight_locus_xy(T)
  d = 0.2562*x - 0.7341*y + 0.0241
  invd = 1.0 / (d if abs(d) > 1e-9 else 1e-9)
  m1 = (-1.7703*x + 5.9114*y - 1.3515) * invd
  m2 = (-31.4424*x + 30.0717*y + 0.0300) * invd
  return m1, m2

def _illuminant_spd_36(cct):
  """Illuminant SPD on the 36-band exposure grid, matching the shader's
  eval_illuminant_spd exactly (blackbody below 4000K, else CIE daylight basis)."""
  if cct < 1.0:
    return np.ones(36)
  if cct < _ILLUMINANT_BB_MAX_CCT:
    return _colour_blackbody(_LAM36, cct)
  m1, m2 = _daylight_weights(cct)
  return _CIE_D_S0_36 + m1 * _CIE_D_S1_36 + m2 * _CIE_D_S2_36

# Spectral upsampling LUT for exposure normalization.
_SPECTRA_EM_LUT = None
def _load_spectra_em_lut():
  global _SPECTRA_EM_LUT
  if _SPECTRA_EM_LUT is None:
    candidates = [
      'data/spectra-em.lut',
      os.path.join(os.environ.get('VKDT_BIN', ''), 'data/spectra-em.lut'),
      os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../../bin/data/spectra-em.lut'),
    ]
    found = next((c for c in candidates if c and os.path.isfile(c)), None)
    if found is None:
      raise FileNotFoundError('spectra-em.lut not found; set VKDT_BIN to the vkdt bin/ directory')
    with open(found, 'rb') as f:
      magic, version, channels, datatype, wd, ht = struct.unpack('<iHBBii', f.read(16))
      assert magic == 1234 and channels == 4 and datatype == 1
      _SPECTRA_EM_LUT = np.frombuffer(f.read(wd * ht * 4 * 4), dtype='<f4').reshape(ht, wd, 4)
  return _SPECTRA_EM_LUT

def _tri2quad(x, y):
  ty = y / max(1.0 - x, 1e-10)
  tx = np.clip((1.0 - x) * (1.0 - x), 0.0, 1.0)
  return tx, np.clip(ty, 0.0, 1.0)

def _reconstruct_white_sd(white_xyz, lam):
  """Neutral spectrum reconstructed by fetch_coeff."""
  lut = _load_spectra_em_lut()
  b = sum(white_xyz)
  x, y = white_xyz[0] / b, white_xyz[1] / b
  tx, ty = _tri2quad(x, y)
  ix = int(np.clip(np.floor(tx * lut.shape[1] + 0.5), 0, lut.shape[1] - 1))
  iy = int(np.clip(np.floor(ty * lut.shape[0] + 0.5), 0, lut.shape[0] - 1))
  A, B, C, w = lut[iy, ix]
  xx = (A * lam + B) * lam + C
  yy = 1.0 / np.sqrt(xx * xx + 1.0)
  return (0.5 * xx * yy + 0.5) * w

def _envelope(w):
  """Shader envelope at the bake-side bands."""
  def smoothstep(edge0, edge1, x):
    t = np.clip((x - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)
  return 1000.0 * smoothstep(380.0, 400.0, w) * (1.0 - smoothstep(700.0, 730.0, w))

def expose_normalization(ref_cct, sensitivity):
  """Per-channel exposure normalization: reflectance reconstructed at D65
  (matching the shader's fixed-D65 fetch_coeff anchor), converted to radiance
  under the film's own reference illuminant. The runtime factor does the same
  conversion for the actual target illuminant (ref_cct, or a simulated film_ill),
  so a neutral scene under the film's own reference stays neutral."""
  white_sd = _reconstruct_white_sd(_D65_XYZ, _LAM36) * _envelope(_LAM36) * _illuminant_spd_36(ref_cct)
  norm = tuple(float(np.sum(sensitivity[:, c] * white_sd)) for c in range(3))
  return tuple(n if n > 1e-8 else 1.0 for n in norm)

# spektrafilm halation presets.
_HALATION_PRESETS = {
    ('still', 'strong'): (0.015, 0.005, 0.0),
    ('still', 'weak'):   (0.08,  0.02,  0.0),
    ('still', 'no'):     (0.30,  0.10,  0.015),
    ('cine',  'strong'): (0.015, 0.005, 0.0),
    ('cine',  'weak'):   (0.08,  0.02,  0.0),
    ('cine',  'no'):     (0.30,  0.10,  0.015),
}

def _resolve(stock_preset, class_defaults, key, schema_default):
  """Resolve stock, class, then schema defaults."""
  val = stock_preset.get(key, class_defaults.get(key, schema_default))
  if isinstance(val, (tuple, list)):
    vals = list(val)
    first = next((v for v in vals if v is not None), schema_default[0])
    vals = [first if v is None else v for v in vals]
    vals += [first] * (len(schema_default) - len(vals))
    return tuple(vals[:len(schema_default)])
  return val

def _model_channel(model, key, c, n_rows):
  """model[key] row for channel c, mono profiles fall back to row 0."""
  return model[key][c if n_rows > c else 0]

def _model_layer_curves(model, logexp, positive):
  """Three-channel density from the Gaussian-CDF model."""
  n_rows = len(model['centers'])
  sign = -1.0 if positive else 1.0
  out = np.zeros((len(logexp), 3, 3))  # [le, sublayer, channel]
  for c in range(3):
    centers = _model_channel(model, 'centers', c, n_rows)
    amps    = _model_channel(model, 'amplitudes', c, n_rows)
    sigs    = _model_channel(model, 'sigmas', c, n_rows)
    for l in range(3):
      z = sign * (logexp - centers[l]) / sigs[l]
      out[:, l, c] = amps[l] * scipy.stats.norm.cdf(z)
  return out

def _coarsest_area_from_curves(density_curves_layers, density_min_layers, density_max_layers,
                                density_max_fractions, particle_scale_sublayers, uniformity,
                                rms_granularity):
  """Coarsest-layer particle area in square micrometres."""
  d_abs = density_curves_layers + density_min_layers[None, :, :]  # [le, sub, ch]
  weight = np.asarray(particle_scale_sublayers, dtype=float)[None, :, None] / density_max_fractions[None, :, :]
  var_shape = weight * d_abs * (density_max_layers[None, :, :] - uniformity[None, None, :] * d_abs)
  peak = np.max(np.sum(var_shape, axis=1), axis=0)  # [channel]
  sigma_in = np.asarray(rms_granularity, dtype=float) / 1000.0
  return sigma_in ** 2 * _RMS_APERTURE_AREA_UM2 / np.maximum(peak, 1e-9)

def compute_physical_constants(info, model):
  """Resolved coupler and grain data for one stock."""
  is_bw = info.channel_model == 'bw'
  is_positive = info.type == 'positive'
  is_cine = info.use == 'cine'
  channel_model = 'bw' if is_bw else 'color'
  polarity = 'positive' if is_positive else 'negative'

  cp_class = dict(_couplers_toml.get('defaults', {}).get(channel_model, {}).get(polarity, {}))
  if channel_model == 'color' and polarity == 'negative' and is_cine:
    ref_ill = str(info.reference_illuminant or '').upper()
    cine_branch = 'tungsten' if ref_ill.startswith('T') else 'daylight'
    cp_class.update(cp_class.get('cine', {}).get(cine_branch, {}))
  cp_class.pop('cine', None)
  cp_stock = _couplers_toml.get(info.stock, {})

  gamma_self  = np.array(_resolve(cp_stock, cp_class, 'gamma_samelayer_rgb', _COUPLER_SCHEMA_DEFAULTS['gamma_samelayer_rgb']), dtype=float)
  gamma_r_gb  = np.array(_resolve(cp_stock, cp_class, 'gamma_interlayer_r_to_gb', _COUPLER_SCHEMA_DEFAULTS['gamma_interlayer_r_to_gb']), dtype=float)
  gamma_g_rb  = np.array(_resolve(cp_stock, cp_class, 'gamma_interlayer_g_to_rb', _COUPLER_SCHEMA_DEFAULTS['gamma_interlayer_g_to_rb']), dtype=float)
  gamma_b_rg  = np.array(_resolve(cp_stock, cp_class, 'gamma_interlayer_b_to_rg', _COUPLER_SCHEMA_DEFAULTS['gamma_interlayer_b_to_rg']), dtype=float)
  inhib_self  = float(_resolve(cp_stock, cp_class, 'inhibition_samelayer', _COUPLER_SCHEMA_DEFAULTS['inhibition_samelayer']))
  inhib_inter = float(_resolve(cp_stock, cp_class, 'inhibition_interlayer', _COUPLER_SCHEMA_DEFAULTS['inhibition_interlayer']))

  # Donor-to-receiver coupler matrix.
  M = np.zeros((3, 3))
  if is_bw:
    M[0, 0] = M[1, 1] = M[2, 2] = gamma_self[0] * inhib_self
  else:
    M[0, 0], M[1, 1], M[2, 2] = gamma_self * inhib_self
    M[0, 1], M[0, 2] = gamma_r_gb * inhib_inter
    M[1, 0], M[1, 2] = gamma_g_rb * inhib_inter
    M[2, 0], M[2, 1] = gamma_b_rg * inhib_inter
  # Pack donor rows as GLSL mat3 columns.

  gr_class = dict(_grain_toml.get('defaults', {}).get(channel_model, {}).get(polarity, {}))
  gr_stock = _grain_toml.get(info.stock, {})
  rms  = _resolve(gr_stock, gr_class, 'rms_granularity', _GRAIN_SCHEMA_DEFAULTS['rms_granularity'])
  dmin = _resolve(gr_stock, gr_class, 'density_min', _GRAIN_SCHEMA_DEFAULTS['density_min'])
  unif = _resolve(gr_stock, gr_class, 'uniformity', _GRAIN_SCHEMA_DEFAULTS['uniformity'])
  pss  = _resolve(gr_stock, gr_class, 'particle_scale_sublayers', _GRAIN_SCHEMA_DEFAULTS['particle_scale_sublayers'])
  dmin = np.asarray(dmin, dtype=float)
  unif = np.asarray(unif, dtype=float)
  rms  = np.asarray(rms, dtype=float)
  pss  = np.asarray(pss, dtype=float)

  # Particle area from the realized multilayer curves.
  logexp = np.linspace(-4.0, 4.0, 256)
  n_rows = len(model['centers'])
  curves_layers = _model_layer_curves(model, logexp, is_positive)  # [le, sub, ch]
  density_max_layers = np.array([  # net (fog-excluded) Dmax, [sublayer, channel]
      [_model_channel(model, 'amplitudes', c, n_rows)[l] for c in range(3)]
      for l in range(3)
  ])
  density_max_total = density_max_layers.sum(axis=0)  # [channel]
  density_max_fractions = density_max_layers / np.maximum(density_max_total[None, :], 1e-9)
  dmin_layers = density_max_fractions * dmin[None, :]  # [sublayer, channel]
  density_max_layers_abs = density_max_layers + dmin_layers

  a1 = _coarsest_area_from_curves(curves_layers, dmin_layers, density_max_layers_abs,
                                   density_max_fractions, pss, unif, rms)  # coarsest (fast) sublayer, per channel
  grain_area = a1[None, :] * pss[:, None]  # [sublayer(fast,mid,slow), channel]

  halation_strength = np.array(_HALATION_PRESETS.get((info.use, info.antihalation), (0.015, 0.005, 0.0)))

  return {
      'M_col0': M[0, :], 'M_col1': M[1, :], 'M_col2': M[2, :],
      'grain_area_fast': grain_area[0], 'grain_area_mid': grain_area[1], 'grain_area_slow': grain_area[2],
      'uniformity': unif,
      'halation_strength': halation_strength,
      'density_min': dmin,
  }

# Stock order is API; keep it in sync with params.ui.
film_stocks = [
    # colour negative, still
    'kodak_gold_200',
    'kodak_ultramax_400',
    'kodak_ektar_100',
    'kodak_portra_160',
    'kodak_portra_400',
    'kodak_portra_800',
    'kodak_portra_800_push1',
    'kodak_portra_800_push2',
    'fujifilm_c200',
    'fujifilm_xtra_400',
    'fujifilm_pro_400h',
    # colour negative, cine
    'kodak_vision3_50d',
    'kodak_vision3_250d',
    'kodak_vision3_200t',
    'kodak_vision3_500t',
    'kodak_verita_200d',
    # colour positive (slide)
    'kodak_kodachrome_64',
    'kodak_ektachrome_100',
    'fujifilm_provia_100f',
    'fujifilm_velvia_100',
    # black & white
    'kodak_doublex',
    'kodak_trix',
]
print_papers = [
    # RA-4 colour paper
    'kodak_endura_premier',
    'kodak_supra_endura',
    'kodak_ultra_endura',
    'kodak_portra_endura',
    'kodak_ektacolor_edge',
    'fujifilm_crystal_archive_typeii',
    # cine print
    'kodak_2383',
    'kodak_2393',
    # black & white
    'kodak_2302',  # bw cine dupe/print stock, monochrome analogue of 2383/2393
]

def _check_stock_lists_in_sync(films, papers):
  """Keep LUT stock order aligned with the UI."""
  here = Path(__file__).resolve().parent
  problems = []

  ui = here / 'params.ui'
  if ui.is_file():
    for key, want in (('film', films), ('paper', papers)):
      line = next((l for l in ui.read_text().splitlines()
                   if l.startswith(key + ':combo:')), None)
      if line is None:
        problems.append(f'params.ui: no {key} combo')
      else:
        n = len(line.split(':')) - 2
        if n != len(want):
          problems.append(f'params.ui: {key} combo has {n} entries, expected {len(want)}')

  head = here / 'head.glsl'
  if head.is_file():
    m = re.search(r'const int s_paper_offset\s*=\s*(\d+)', head.read_text())
    if not m:
      problems.append('head.glsl: s_paper_offset not found')
    elif int(m.group(1)) != len(films):
      problems.append(f'head.glsl: s_paper_offset is {m.group(1)}, expected {len(films)}')

  wb = here / 'wb.h'
  if wb.is_file():
    txt = wb.read_text()
    for key, want in (('films', films), ('papers', papers)):
      m = re.search(key + r'=\((.*?)\)', txt, re.S)
      if not m:
        problems.append(f'wb.h: {key}=() not found')
      elif m.group(1).split() != list(want):
        problems.append(f'wb.h: {key}=() does not match this script\'s list')
    m = re.search(r'const float wb\[(\d+)\]\[(\d+)\]\[4\]', txt)
    if not m:
      problems.append('wb.h: wb[][][] table not found')
    elif (int(m.group(1)), int(m.group(2))) != (len(films), len(papers)):
      problems.append(f'wb.h: table is [{m.group(1)}][{m.group(2)}], '
                      f'expected [{len(films)}][{len(papers)}]')

  if problems:
    raise SystemExit('stock lists out of sync:\n  ' + '\n  '.join(problems)
                     + '\nfix those before regenerating, or the lut and the ui disagree.')

_check_stock_lists_in_sync(film_stocks, print_papers)

# start lut files:
print('number of film stocks/paper offset:', len(film_stocks))
num_films = len(film_stocks) + len(print_papers)
f_lut = open("filmsim.lut", "wb")  # unified all data in 0..255, padded with zeros
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 256, 3*num_films)
f_lut.write(header)

n_film_stocks = len(film_stocks)  # film combo range; papers follow
film_stocks.extend(print_papers)

for f in film_stocks:
  print(f)

  profile = DotMap()
  with open(_PROFILES_DIR / (f + '.json'), "rb") as file:
    profile = DotMap(json.load(file))

    # B&W development-time columns use one representative index.
    dev_times = profile.data.development_time
    if dev_times is not None and len(dev_times) > 1:
      dev_idx = (len(dev_times) - 1) // 2
      dc = np.array(profile.data.density_curves)
      profile.data.density_curves = dc[:, dev_idx:dev_idx + 1].tolist()
      model = profile.data.density_curves_model
      if model:
        for key in ('centers', 'amplitudes', 'sigmas', 'alphas'):
          vals = model.get(key)
          if vals:
            model[key] = [vals[dev_idx]]
    else:
      dev_idx = 0

    # Nx3
    profile.data.log_sensitivity = np.array(profile.data.log_sensitivity)
    # pad grey up to rgb
    if np.shape(profile.data.log_sensitivity)[1] == 1:
      profile.data.log_sensitivity = np.repeat(profile.data.log_sensitivity, 3, axis=1)
    print(np.shape(profile.data.log_sensitivity)) # (41, 3) or (81, 3)

    for i in range(0, np.shape(profile.data.log_sensitivity)[0]):
      px = struct.pack('<ffff',
          float('NaN') if profile.data.log_sensitivity[i][0] is None else profile.data.log_sensitivity[i][0],
          float('NaN') if profile.data.log_sensitivity[i][1] is None else profile.data.log_sensitivity[i][1],
          float('NaN') if profile.data.log_sensitivity[i][2] is None else profile.data.log_sensitivity[i][2],
          1)
      f_lut.write(px)
    for i in range(np.shape(profile.data.log_sensitivity)[0], 256):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_lut.write(px)

    # Nx5 but Nx4 is enough for us (C M Y min_densitiy)
    profile.data.channel_density= np.array(profile.data.channel_density)
    # Keep mono dye in one channel to avoid triple counting.
    if np.shape(profile.data.channel_density)[1] == 1:
      profile.data.channel_density = np.pad(profile.data.channel_density,
                                            ((0, 0), (0, 2)), constant_values=0.0)
    profile.data.base_density = np.array(profile.data.base_density)
    print(np.shape(profile.data.channel_density)) # (41, 3) or (81, 3)
    if profile.data.base_density.ndim > 1:
      # column axis is development_time (see dev_idx above), not channel.
      profile.data.base_density = profile.data.base_density[:,dev_idx]
    print(np.shape(profile.data.base_density)) # (41) or (81)
    for i in range(0, np.shape(profile.data.channel_density)[0]):
      px = struct.pack('<ffff',
          float('NaN') if profile.data.channel_density[i][0] is None else profile.data.channel_density[i][0],
          float('NaN') if profile.data.channel_density[i][1] is None else profile.data.channel_density[i][1],
          float('NaN') if profile.data.channel_density[i][2] is None else profile.data.channel_density[i][2],
          float('NaN') if profile.data.base_density[i] is None else profile.data.base_density[i])
      f_lut.write(px)
    for i in range(np.shape(profile.data.channel_density)[0], 256):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_lut.write(px)


    # Texels 0-11: layer centers, amplitudes, inverse sigmas, and alphas.
    # Texels 12-22: stock, coupler, grain, halation, and exposure data.
    model = profile.data.density_curves_model
    assert model and 'centers' in model and len(model['centers']) > 0, \
        f"{f}: missing density_curves_model"
    n_layers = len(model['centers'][0])
    assert n_layers == 3, f"{f}: expected 3 density_curves_model layers, got {n_layers}"

    # Sort sublayers by ascending center.
    for c in range(len(model['centers'])):
      order = np.argsort(np.asarray(model['centers'][c], dtype=float))
      model['centers'][c]     = [model['centers'][c][i] for i in order]
      model['amplitudes'][c]  = [model['amplitudes'][c][i] for i in order]
      model['sigmas'][c]      = [model['sigmas'][c][i] for i in order]
      if model.get('alphas'):
        model['alphas'][c] = [model['alphas'][c][i] for i in order]

    dens_model = np.zeros((256, 4))

    for l in range(3):
      centers = [model['centers'][c if len(model['centers']) > c else 0][l] for c in range(3)]
      amps = [model['amplitudes'][c if len(model['amplitudes']) > c else 0][l] for c in range(3)]
      sigs = [model['sigmas'][c if len(model['sigmas']) > c else 0][l] for c in range(3)]
      inv_sigs = [1.0 / max(float(sig), 1e-3) for sig in sigs]
      alphas = [model['alphas'][c if len(model['alphas']) > c else 0][l] for c in range(3)] if 'alphas' in model and model['alphas'] else [0, 0, 0]
      dens_model[l*4 + 0, 0:3] = centers
      dens_model[l*4 + 1, 0:3] = amps
      dens_model[l*4 + 2, 0:3] = inv_sigs
      dens_model[l*4 + 3, 0:3] = alphas

    # Film-only texels stay zero for papers.
    if profile.info.support == 'film':
      # Use the model asymptote as density maximum.
      density_max = np.array([
          sum(_model_channel(model, 'amplitudes', c, len(model['centers']))[l]
              for l in range(3))
          for c in range(3)])
      phys = compute_physical_constants(profile.info, model)
      dens_model[12, 0:3] = density_max
      dens_model[12, 3]   = 1.0 if profile.info.type == 'positive' else 0.0
      dens_model[13, 0:3] = phys['M_col0']
      dens_model[14, 0:3] = phys['M_col1']
      dens_model[15, 0:3] = phys['M_col2']
      dens_model[16, 0:3] = phys['grain_area_fast']
      dens_model[17, 0:3] = phys['grain_area_mid']
      dens_model[18, 0:3] = phys['grain_area_slow']
      dens_model[19, 0:3] = phys['uniformity']
      dens_model[20, 0:3] = phys['halation_strength']
      dens_model[21, 0:3] = phys['density_min']
      ref_ill = str(profile.info.reference_illuminant or 'D55').upper()
      assert ref_ill in _REF_ILLUMINANT_CCT, \
          f"{f}: unknown reference_illuminant {ref_ill!r}, add its signed CCT to _REF_ILLUMINANT_CCT"
      ref_cct = _REF_ILLUMINANT_CCT[ref_ill]
      dens_model[22, 3] = ref_cct

      lam36 = 380.0 + 10.0 * np.arange(36)
      sens36 = np.stack([
          np.interp(lam36, np.linspace(380.0, 780.0, profile.data.log_sensitivity.shape[0]),
                    profile.data.log_sensitivity[:, c].astype(float))
          for c in range(3)], axis=1)
      sens36 = np.nan_to_num(10.0 ** sens36, nan=0.0)  # NaN bands drop out, matching the shader's NaN handling
      dens_model[22, 0:3] = expose_normalization(ref_cct, sens36)

    for i in range(0, 256):
      px = struct.pack('<ffff',
          dens_model[i][0],
          dens_model[i][1],
          dens_model[i][2],
          dens_model[i][3])
      f_lut.write(px)

    del profile.data.density_curves
    del profile.data.base_density
    del profile.data.channel_density
f_lut.write("""license:
spektrafilm profile by Andrea Volpato, licensed under CC BY-SA 4.0.
Redistribution and derivatives must credit the author, link the project
(https://github.com/andreavolpato/spektrafilm), preserve this license,
and remain CC BY-SA 4.0. Modifications must be noted. Full text of the
license and attribution requirements:
https://github.com/andreavolpato/spektrafilm/blob/main/SPEKTRAFILM_LICENSE.txt.""".encode('utf-8'))
f_lut.write("""

citation:
If you use this profile in your work, please cite the spektrafilm
project: https://github.com/andreavolpato/spektrafilm, see CITATION.cff
for details.""".encode('utf-8'))
f_lut.write("""

datasource:
This profile was created by processing raw measurement data from
data-sheets and/or scientific papers. Original data are property of the respective holders.
Film/photo-paper: Kodak and Fujifilm data-sheets, scientific publications, and technical material.
Reflectance: Otsu (https://github.com/enneract/otsu2018),
Munsell (https://zenodo.org/records/3269912),
human skin (https://www.nist.gov/programs-projects/reflectance-measurements-human-skin),
forest colors (https://zenodo.org/records/3269920),
Japan colors (https://zenodo.org/records/5217752).
All data publicly available.""".encode('utf-8'))
f_lut.write("""

modifications:
this datafile has been generated from the spektrafilm profile/*json
using a half assed python script found here:
https://codeberg.org/hanatos/vkdt/src/branch/master/src/pipe/modules/filmsim/mklut-profiles.py
""".encode('utf-8'))
f_lut.close()
