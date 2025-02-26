import json
from os import listdir
from os.path import isfile, join, splitext
from dotmap import DotMap
import numpy as np
import struct

film_stocks = [
    'kodak_portra_400_auc',
    'kodak_ultramax_400_auc',
    'kodak_gold_200_auc',
    'kodak_vision3_50d_uc',
    'fujifilm_pro_400h_auc',
    'fujifilm_xtra_400_auc',
    'fujifilm_c200_auc',
]
print_papers = [
    'kodak_endura_premier_uc',
    'kodak_ektacolor_edge_uc',
    'kodak_supra_endura_uc',
    'kodak_portra_endura_uc',
    'fujifilm_crystal_archive_typeii_uc',
]

# start lut files:
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
    print(np.shape(profile.data.log_sensitivity)) # (41, 3) or (81, 3)

    for i in range(0, np.shape(profile.data.log_sensitivity)[0]):
      px = struct.pack('<ffff',
          profile.data.log_sensitivity[i][0],
          profile.data.log_sensitivity[i][1],
          profile.data.log_sensitivity[i][2],
          1)
      f_lut.write(px)
    for i in range(np.shape(profile.data.log_sensitivity)[0], 256):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_lut.write(px)

    # Nx5 but Nx4 is enough for us (C M Y min_densitiy)
    profile.data.dye_density = np.array(profile.data.dye_density)
    print(np.shape(profile.data.dye_density)) # (41, 5) or (81, 5)
    for i in range(0, np.shape(profile.data.dye_density)[0]):
      px = struct.pack('<ffff',
          profile.data.dye_density[i][0],
          profile.data.dye_density[i][1],
          profile.data.dye_density[i][2],
          profile.data.dye_density[i][3])
      f_lut.write(px)
    for i in range(np.shape(profile.data.dye_density)[0], 256):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_lut.write(px)


    # Mx3
    # normalise to -4..4, all the same
    profile.data.density_curves = np.array(profile.data.density_curves)
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
f_lut.close();
