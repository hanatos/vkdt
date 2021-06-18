#!/usr/bin/python3
# parse an icc profile (which you created using displaycal or so)
# and output matrix + gamma values for transformation in the fragment shader.
import os, sys
import numpy

if len(sys.argv) != 2:
  print('please supply icc profile as command line argument')
  print('usage: ./read-icc.py your-display.icc')
  sys.exit(1)

profile=sys.argv[1]

# from PIL import ImageCms
# XXX how to get the data? the docs lie and we get a useless ImageCmsProfile
# instead of a CmsProfile object here
# prf = ImageCms.getOpenProfile(profile)
# print(prf)

# probably easier to go the bash way:
# bradford adaptation
proc = os.popen('iccdump  -v3 -t rTRC -t gTRC -t bTRC '+profile, 'r', 1)
got = proc.read().replace(',', '')
proc.close()
tokens = got.split()
gamma = numpy.array([
float(tokens[5]),
float(tokens[11]),
float(tokens[17])])

proc = os.popen('iccdump -v3 -t wtpt -t rXYZ -t gXYZ -t bXYZ '+profile, 'r', 1)
got = proc.read().replace(',', '')
proc.close()
tokens = got.split()
wt = numpy.array([
float(tokens[6]), float(tokens[7]), float(tokens[8])])

A = numpy.matrix([
[float(tokens[19]), float(tokens[20]), float(tokens[21])],
[float(tokens[32]), float(tokens[33]), float(tokens[34])],
[float(tokens[45]), float(tokens[46]), float(tokens[47])]]).T

bradford = numpy.matrix([
[ 0.8951000,   0.2664000, -0.1614000],
[-0.7502000,   1.7135000,  0.0367000],
[ 0.0389000,  -0.0685000,  1.0296000]])

B = bradford @ A
#  D50   0.96422   1.00000   0.82521
#  D55   0.95682   1.00000   0.92149
#  D65   0.95047   1.00000   1.08883
# we need to remove the D50 white from the icc and apply ours
white = numpy.array([0.96422, 1.00000, 0.82521])
white_lms = bradford @ white
wt_lms    = bradford @ wt

C = numpy.diag(numpy.ravel(wt_lms / white_lms)) @ B
M = bradford.I @ C

# used for debugging/sanity checking
# xyz_to_srgb = numpy.matrix([ [ 3.2404542, -1.5371385, -0.4985314], [-0.9692660,  1.8760108,  0.0415560], [ 0.0556434, -0.2040259,  1.0572252] ])
# xyz_to_rec2020 = numpy.matrix([ [ 1.7166511880, -0.3556707838, -0.2533662814], [-0.6666843518,  1.6164812366,  0.0157685458], [ 0.0176398574, -0.0427706133,  0.9421031212] ])

xyz_to_dspy = M.I

xyz_to_rec2020 = numpy.matrix([
[ 1.7166511880, -0.3556707838, -0.2533662814],
[-0.6666843518,  1.6164812366,  0.0157685458],
[ 0.0176398574, -0.0427706133,  0.9421031212],
])

O = xyz_to_dspy * xyz_to_rec2020.I
print('rec2020 to display matrix')
print(O)
print('gamma')
print(gamma)

out = open('display.profile', 'w')
out.write('%f %f %f\n'%(1.0/gamma[0], 1.0/gamma[1], 1.0/gamma[2]))
out.write('%f %f %f\n'%(O[0,0], O[0,1], O[0,2]))
out.write('%f %f %f\n'%(O[1,0], O[1,1], O[1,2]))
out.write('%f %f %f\n'%(O[2,0], O[2,1], O[2,2]))
out.close()

