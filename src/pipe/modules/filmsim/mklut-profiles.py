import json
from os import listdir
from os.path import isfile, join, splitext
from dotmap import DotMap
import numpy as np
import struct

film_stocks = [
    'kodak_ektar_100',
    'kodak_portra_160',
    'kodak_portra_400',
    'kodak_portra_800',
    'kodak_portra_800_push1',
    'kodak_portra_800_push2',
    'kodak_gold_200',
    'kodak_ultramax_400',
    'kodak_vision3_50d',
    'kodak_vision3_250d',
    'kodak_vision3_200t',
    'kodak_vision3_500t',
    'fujifilm_pro_400h',
    'fujifilm_xtra_400',
    'fujifilm_c200',
    'fujifilm_provia_100f',
    'fujifilm_velvia_100',
    'kodak_ektachrome_100',
    'kodak_kodachrome_64',
    'kodak_verita_200d',
    'kodak_2302',
    'kodak_doublex',
    'kodak_trix',
]
print_papers = [
    'kodak_endura_premier',
    'kodak_ektacolor_edge',
    'kodak_supra_endura',
    'kodak_portra_endura',
    'fujifilm_crystal_archive_typeii',
    'kodak_2383',
    'kodak_2393',
    'kodak_ultra_endura',
]

# start lut files:
print('number of film stocks/paper offset:', len(film_stocks))
num_films = len(film_stocks) + len(print_papers)
f_lut = open("filmsim.lut", "wb")  # unified all data in 0..255, padded with zeros
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 256, 3*num_films)
f_lut.write(header)

film_stocks.extend(print_papers)

for f in film_stocks:
  print(f)

  profile = DotMap()
  with open(f + '.json', "rb") as file:
    profile = DotMap(json.load(file))
    # Nx3
    profile.data.log_sensitivity = np.array(profile.data.log_sensitivity)
    # pad grey up to rgb
    if np.shape(profile.data.log_sensitivity)[1] == 1:
      print("replicating colours!!")
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
    # pad grey up to rgb
    if np.shape(profile.data.channel_density)[1] == 1:
      print("replicating colours!!")
      profile.data.channel_density = np.repeat(profile.data.channel_density, 3, axis=1)
    profile.data.base_density = np.array(profile.data.base_density)
    print(np.shape(profile.data.channel_density)) # (41, 3) or (81, 3)
    if profile.data.base_density.ndim > 1:
      print("killing surplus base density values!!")
      profile.data.base_density = profile.data.base_density[:,0]
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


    # Mx3
    # normalise to -4..4, all the same
    profile.data.density_curves = np.array(profile.data.density_curves)
    # pad grey up to rgb
    shp = np.shape(profile.data.density_curves)
    if shp[1] == 1:
      print("replicating colours!!")
      profile.data.density_curves = np.repeat(profile.data.density_curves.reshape(shp[0], 1), 3, axis=1)
    profile.data.log_exposure = np.array(profile.data.log_exposure)
    logexp = np.linspace(-4.0, 4.0, 256)
    dens   = np.zeros((256, 3))
    for channel in np.arange(3):
      sel = ~np.isnan(profile.data.density_curves[:,channel])
      dens[:,channel] = np.interp(logexp[:],
                                             profile.data.log_exposure[sel],
                                             profile.data.density_curves[sel,channel])

    print(np.shape(profile.data.density_curves)) # (256, 3)
    for i in range(0, np.shape(profile.data.density_curves)[0]):
      px = struct.pack('<ffff',
          dens[i][0],
          dens[i][1],
          dens[i][2],
          1)
      f_lut.write(px)
    del profile.data.density_curves
    del profile.data.base_density
    del profile.data.channel_density
f_lut.close()
# python is a piece of shit and that's why we have to re-open the file now:
f_lut = open("filmsim.lut", "a")
f_lut.write("""license:
spektrafilm profile by Andrea Volpato, licensed under CC BY-SA 4.0.
Redistribution and derivatives must credit the author, link the project
(https://github.com/andreavolpato/spektrafilm), preserve this license,
and remain CC BY-SA 4.0. Modifications must be noted. Full text of the
license and attribution requirements:
https://github.com/andreavolpato/spektrafilm/blob/main/SPEKTRAFILM_LICENSE.txt.""")
f_lut.write("""

citation:
If you use this profile in your work, please cite the spektrafilm
project: https://github.com/andreavolpato/spektrafilm, see CITATION.cff
for details.""")
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
All data publicly available.""")
f_lut.write("""

modifications:
this datafile has been generated from the spektrafilm profile/*json
using a half assed python script found here:
https://codeberg.org/hanatos/vkdt/src/branch/master/src/pipe/modules/filmsim/mklut-profiles.py
""")
f_lut.close()
