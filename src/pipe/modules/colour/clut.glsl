// img_clut is declared by the including shader; n==2 uses the legacy layout.

int clut_nanchor(int nbands)
{
  return (nbands * 2) / 3;
}

vec2 clut_chroma(vec2 tc, int idx, int nbands)
{
  int band = (nbands == 3) ? 2*idx : idx;
  return texture(img_clut, vec2((tc.x + float(band)) / float(nbands), tc.y)).xy;
}

float clut_luminance(vec2 tc, int idx, int n, int nbands)
{
  if(nbands == 3)
    return texture(img_clut, vec2((tc.x + 1.0) / 3.0, tc.y))[idx];
  vec2 pair = texture(img_clut, vec2((tc.x + float(n + idx/2)) / float(nbands), tc.y)).xy;
  return (idx % 2 == 0) ? pair.x : pair.y;
}
