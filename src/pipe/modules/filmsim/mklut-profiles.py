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

def _write_rows(fh, rows, tail):
  """Emit rows as rgba f32, NaN for missing channels, zero-padded to 256."""
  def _f(v):
    return float('NaN') if v is None else v
  n = np.shape(rows)[0]
  for i in range(n):
    fh.write(struct.pack('<ffff', _f(rows[i][0]), _f(rows[i][1]), _f(rows[i][2]), _f(tail(i))))
  for i in range(n, 256):
    fh.write(struct.pack('<ffff', 0, 0, 0, 1))


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
num_stocks = len(film_stocks) + len(print_papers)
f_lut = open("filmsim.lut", "wb")  # unified all data in 0..255, padded with zeros
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 256, 3*num_stocks)
f_lut.write(header)

film_stocks.extend(print_papers)  # film combo range first, papers follow

for f in film_stocks:
  print(f)

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
        for key in ('centers', 'amplitudes', 'sigmas'):
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

    _write_rows(f_lut, profile.data.log_sensitivity, lambda i: 1)

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
    _write_rows(f_lut, profile.data.channel_density,
                lambda i: profile.data.base_density[i])


    # Texels 0-11: layer centers, amplitudes and inverse sigmas, one layer per
    # group of four; the fourth texel of each group is unused.
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

    dens_model = np.zeros((256, 4))

    for l in range(3):
      centers = [model['centers'][c if len(model['centers']) > c else 0][l] for c in range(3)]
      amps = [model['amplitudes'][c if len(model['amplitudes']) > c else 0][l] for c in range(3)]
      sigs = [model['sigmas'][c if len(model['sigmas']) > c else 0][l] for c in range(3)]
      inv_sigs = [1.0 / max(float(sig), 1e-3) for sig in sigs]
      dens_model[l*4 + 0, 0:3] = centers
      dens_model[l*4 + 1, 0:3] = amps
      dens_model[l*4 + 2, 0:3] = inv_sigs

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
