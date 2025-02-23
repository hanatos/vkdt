# run in data/filters/dichroics/thorlabs
import struct
import csv
import numpy as np

col = np.zeros((451, 3))
print(col)
with open('filter_c.csv', newline='') as csvfile:
  rd = csv.reader(csvfile, delimiter=',', quotechar='|')
  i = 0
  for row in rd:
    col[i][0] = float(row[1])
    print(row[1])
    i = i+1
with open('filter_m.csv', newline='') as csvfile:
  rd = csv.reader(csvfile, delimiter=',', quotechar='|')
  i = 0
  for row in rd:
    col[i][1] = float(row[1])
    i = i+1
with open('filter_y.csv', newline='') as csvfile:
  rd = csv.reader(csvfile, delimiter=',', quotechar='|')
  i = 0
  for row in rd:
    col[i][2] = float(row[1])
    i = i+1

print(col)

f = open("thorlabs.lut", "wb")
header = struct.pack('<iHBBii', 1234, 2, 4, 1, 451, 1)
f.write(header)
for i in range(451):
  px = struct.pack('<ffff',
      col[i][0],
      col[i][1],
      col[i][2],
      1)
  f.write(px)
f.close();
