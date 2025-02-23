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
f_log_sensitivity = open("log_sensitivity.lut", "wb") # spectral
f_dye_density     = open("dye_density.lut", "wb")     # spectral
f_density_curves  = open("density_curves.lut", "wb")  # -4..4 log exposure lut
f_density_layers  = open("density_layers.lut", "wb")  # 0..3.3 density lut, 3x3 per item
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 81, num_films)
f_log_sensitivity.write(header)
f_dye_density.write(header)
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 256, num_films)
f_density_curves.write(header)
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 256, 3*num_films)
f_density_layers.write(header)
# log_sensitivity (81, 3)
# dye_density     (81, 4)
# density_curves (256, 3)

film_stocks.extend(print_papers)

for f in film_stocks:
  print(f)

  profile = DotMap()
  with open(f + '.json', "rb") as file:
    profile = DotMap(json.load(file))
    # Nx3
    profile.data.log_sensitivity = np.array(profile.data.log_sensitivity)
    print(np.shape(profile.data.log_sensitivity)) # (41, 3) or (81, 3)
    # TODO something extend to 5nm steps always:
    # if np.shape(profile.data.log_sensitivity)[0] < 80:
    #       xo = np.linspace(0, 80, 41)
    #       xn = np.linspace(0, 80, 81)
    #       profile.data.log_sensitivity = np.interp(xn, xo, profile.data.log_sensitivity)
    #       print(np.shape(profile.data.log_sensitivity)) # (41, 3) or (81, 3)
    # Nx5 but Nx4 is enough for us (C M Y min_densitiy)

    for i in range(0, np.shape(profile.data.log_sensitivity)[0]):
      px = struct.pack('<ffff',
          profile.data.log_sensitivity[i][0],
          profile.data.log_sensitivity[i][1],
          profile.data.log_sensitivity[i][2],
          1)
      f_log_sensitivity.write(px)
    # FIXME: pad in between, not at the end!
    for i in range(np.shape(profile.data.log_sensitivity)[0], 81):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_log_sensitivity.write(px)

    profile.data.dye_density = np.array(profile.data.dye_density)
    print(np.shape(profile.data.dye_density)) # (41, 5) or (81, 5)
    for i in range(0, np.shape(profile.data.dye_density)[0]):
      px = struct.pack('<ffff',
          profile.data.dye_density[i][0],
          profile.data.dye_density[i][1],
          profile.data.dye_density[i][2],
          profile.data.dye_density[i][3])
      f_dye_density.write(px)
    # FIXME: pad in between, not at the end!
    for i in range(np.shape(profile.data.dye_density)[0], 81):
      px = struct.pack('<ffff', 0, 0, 0, 1)
      f_dye_density.write(px)


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
      f_density_curves.write(px)
    # shape stuff, is always the same i hope:
    # profile.data.wavelengths = np.array(profile.data.wavelengths)


    # density_curves_layers
    # Mx3x3, needed for grain synth:
    # these are read out by interpolating density_cmy along density_curves above
    # density is 0..3.3
    # print(profile.data.density_curves[0])
    # print(profile.data.density_curves[255])
    # print(profile.data.density_curves_layers)
    print(np.shape(profile.data.density_curves_layers)) # (256, 3, 3)
    profile.data.density_curves_layers = np.array(profile.data.density_curves_layers)

    # resample to common range 0..3.3
    densx  = np.linspace(0.0, 3.3, 256)
    layers = np.zeros((256, 3, 3))
    for c0 in np.arange(3):
      for c1 in np.arange(3):
        sel = ~np.isnan(profile.data.density_curves[:,c0])
        layers[:,c0,c1] = np.interp(densx[:],
                                             profile.data.density_curves[sel,c0],
                                             profile.data.density_curves_layers[sel,c0,c1])

    for layer in np.arange(3):
      for i in range(0, np.shape(profile.data.density_curves_layers)[0]):
        px = struct.pack('<ffff',
          layers[i,0,layer],
          layers[i,1,layer],
          layers[i,2,layer],
          1)
        f_density_layers.write(px)

f_log_sensitivity.close();
f_dye_density.close();
f_density_curves.close();
f_density_layers.close();
