from PIL import Image
import struct
import json
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

emptybox = {
  'left' : 0,
  'right' : 0,
  'bottom' : 0,
  'top' : 0
}
with open("font_metrics.lut", "wb") as lut:
  glyph_cnt = 0
  metrics = 11
  header = struct.pack('<iHBBii', 1234, 2, 1, 1, glyph_cnt, metrics)
  lut.write(header)
  with open('metrics.json', 'r') as j:
    m = json.load(j)
    for v in m['variants']:
      for g in v['glyphs']:
        try:
          metr = struct.pack('<Iffffffffff',
            int(g['unicode']),
            float(g['advance']),
            1.0/float(v['metrics']['lineHeight']),
            float(g.get('planeBounds', emptybox)['left']),
            float(g.get('planeBounds', emptybox)['bottom']),
            float(g.get('planeBounds', emptybox)['right'])-float(g.get('planeBounds', emptybox)['left']),
            float(g.get('planeBounds', emptybox)['top']) - float(g.get('planeBounds', emptybox)['bottom']),
            float(g.get('atlasBounds', emptybox)['left'])/width,
            float(g.get('atlasBounds', emptybox)['bottom'])/height,
            (float(g.get('atlasBounds', emptybox)['right'])-float(g.get('atlasBounds', emptybox)['left']))/width,
            (float(g.get('atlasBounds', emptybox)['top']) - float(g.get('atlasBounds', emptybox)['bottom']))/height)
          glyph_cnt=glyph_cnt+1
          lut.write(metr)
        except KeyError as error:
          print("glyph has no unicode")

  print("writing %d glyphs"%glyph_cnt)
  lut.seek(0)
  header = struct.pack('<iHBBii', 1234, 2, 1, 1, glyph_cnt, metrics)
  lut.write(header)
