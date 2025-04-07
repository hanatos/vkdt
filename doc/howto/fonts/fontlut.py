from PIL import Image
import struct
import csv
import numpy as np

width = 0
height = 0
with Image.open("atlas.png") as img:
  with open("font_msdf.lut", "wb") as lut:
    width,height = img.size
    header = struct.pack('<iHBBii', 1234, 2, 3, 2, width, height)
    lut.write(header)
    lut.write(np.array(img).astype('int8').tobytes())
# could append some plain text metadata to the lut here

with open("font_metrics.lut", "wb") as lut:
  glyph_cnt = 0
  metrics = 10
  header = struct.pack('<iHBBii', 1234, 2, 1, 1, glyph_cnt, metrics)
  lut.write(header)

  # first column is font id (we skip this)
  # 4x box in em/screen space
  # 4x box in px/uv texture space
  with open('metrics.csv', 'r') as f:
    r = csv.reader(f, delimiter=',')
    for row in r:
      metr = struct.pack('<Ifffffffff',
          int(row[1]),   # codepoint
          float(row[2]), # advance 
          float(row[3]), float(row[4]), (float(row[5])-float(row[3])), (float(row[ 6])-float(row[4])),
          float(row[7])/width, float(row[8])/height, (float(row[9])-float(row[7]))/width, (float(row[10])-float(row[8]))/height)
      glyph_cnt=glyph_cnt+1
      lut.write(metr)

  lut.seek(0)
  header = struct.pack('<iHBBii', 1234, 2, 1, 1, glyph_cnt, metrics)
  lut.write(header)
