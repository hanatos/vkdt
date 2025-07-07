/*
 * HSLuv-C: Human-friendly HSL
 * <https://github.com/hsluv/hsluv-c>
 * <https://www.hsluv.org/>
 *
 * Copyright (c) 2015 Alexei Boronine (original idea, JavaScript implementation)
 * Copyright (c) 2015 Roger Tallada (Obj-C implementation)
 * Copyright (c) 2017 Martin Mitáš (C implementation, based on Obj-C implementation)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */


static inline void
hsluv_get_bounds(float l, float bounds[6][2])
{
  const float kappa = 903.29629629629629629630f;
  const float epsilon = 0.00885645167903563082f;
  float tl = l + 16.0f;
  float sub1 = (tl * tl * tl) / 1560896.0f;
  float sub2 = (sub1 > epsilon ? sub1 : (l / kappa));

  const float xyz_to_rec2020[][3] = {
    { 1.71665119, -0.66668435,  0.01763986},
    {-0.35567078,  1.61648124, -0.04277061},
    {-0.25336628,  0.01576855,  0.94210312}};

  for(int channel = 0; channel < 3; channel++)
  {
    float m1 = xyz_to_rec2020[channel][0];
    float m2 = xyz_to_rec2020[channel][1];
    float m3 = xyz_to_rec2020[channel][2];

    for (int t = 0; t < 2; t++) {
      float top1 = (284517.0 * m1 - 94839.0 * m3) * sub2;
      float top2 = (838422.0 * m3 + 769860.0 * m2 + 731718.0 * m1) * l * sub2 -  769860.0 * t * l;
      float bottom = (632260.0 * m3 - 126452.0 * m2) * sub2 + 126452.0 * t;

      bounds[channel * 2 + t][0] = top1 / bottom;
      bounds[channel * 2 + t][1] = top2 / bottom;
    }
  }
}

static inline float
hsluv_ray_length_until_intersect(float theta, const float* line)
{
  return line[1] / (sinf(theta) - line[0] * cosf(theta));
}

static inline float
hsluv_max_chroma_for_lh(float l, float h)
{
  float min_len = FLT_MAX;
  float hrad = h * 0.01745329251994329577f; /* (2 * pi / 360) */
  float bounds[6][2];

  hsluv_get_bounds(l, bounds);
  for(int i = 0; i < 6; i++)
  {
    float len = hsluv_ray_length_until_intersect(hrad, bounds[i]);
    if(len >= 0  &&  len < min_len)
      min_len = len;
  }
  return min_len;
}

/* https://en.wikipedia.org/wiki/CIELUV
 * In these formulas, Yn refers to the reference white point. We are using
 * illuminant D65, so Yn (see refY in Maxima file) equals 1. The formula is
 * simplified accordingly.
 */
static inline float
y2l(float y)
{
  const float kappa = 903.29629629629629629630f;
  const float epsilon = 0.00885645167903563082f;
  if(y <= epsilon) return y * kappa;
  return 116.0f * powf(y, 1.0f/3.0f) - 16.0f;
}

static inline float
l2y(float l)
{
  const float kappa = 903.29629629629629629630f;
  if(l <= 8.0f) return l / kappa;
  float x = (l + 16.0f) / 116.0f;
  return (x * x * x);
}

static inline void
xyz2luv(float *in_out)
{
  const float ref_u = 0.19783000664283680764f;
  const float ref_v = 0.46831999493879100370f;

  float var_u = (4.0 * in_out[0]) / (in_out[0] + (15.0 * in_out[1]) + (3.0 * in_out[2]));
  float var_v = (9.0 * in_out[1]) / (in_out[0] + (15.0 * in_out[1]) + (3.0 * in_out[2]));
  float l = y2l(in_out[1]);
  float u = 13.0 * l * (var_u - ref_u);
  float v = 13.0 * l * (var_v - ref_v);

  in_out[0] = l;
  if(l < 0.00000001) {
    in_out[1] = 0.0;
    in_out[2] = 0.0;
  } else {
    in_out[1] = u;
    in_out[2] = v;
  }
}

static inline void
luv2xyz(float *in_out)
{
  const float ref_u = 0.19783000664283680764f;
  const float ref_v = 0.46831999493879100370f;

  if(in_out[0] <= 0.00000001) {
    /* Black will create a divide-by-zero error. */
    in_out[0] = 0.0;
    in_out[1] = 0.0;
    in_out[2] = 0.0;
    return;
  }

  float var_u = in_out[1] / (13.0 * in_out[0]) + ref_u;
  float var_v = in_out[2] / (13.0 * in_out[0]) + ref_v;
  float y = l2y(in_out[0]);
  float x = -(9.0 * y * var_u) / ((var_u - 4.0) * var_v - var_u * var_v);
  float z = (9.0 * y - (15.0 * var_v * y) - (var_v * x)) / (3.0 * var_v);
  in_out[0] = x;
  in_out[1] = y;
  in_out[2] = z;
}

static inline void
luv2lch(float *in_out)
{
  float l = in_out[0];
  float u = in_out[1];
  float v = in_out[2];
  float h;
  float c = sqrtf(u * u + v * v);

  /* Grays: disambiguate hue */
  if(c < 0.00000001) {
    h = 0;
  } else {
    h = atan2f(v, u) * 57.29577951308232087680;  /* (180 / pi) */
    if(h < 0.0) h += 360.0;
  }

  in_out[0] = l;
  in_out[1] = c;
  in_out[2] = h;
}

static inline void
lch2luv(float *in_out)
{
  float hrad = in_out[2] * 0.01745329251994329577;  /* (pi / 180.0) */
  float u = cosf(hrad) * in_out[1];
  float v = sinf(hrad) * in_out[1];

  in_out[1] = u;
  in_out[2] = v;
}

static inline void
hsluv2lch(float *in_out)
{
  float h = in_out[0];
  float s = in_out[1];
  float l = in_out[2];
  float c;

  /* White and black: disambiguate chroma */
  if(l > 99.9999999 || l < 0.00000001)
    c = 0.0;
  else
    c = hsluv_max_chroma_for_lh(l, h) / 100.0 * s;

  /* Grays: disambiguate hue */
  if (s < 0.00000001)
    h = 0.0;

  in_out[0] = l;
  in_out[1] = c;
  in_out[2] = h;
}

static inline void
lch2hsluv(float *in_out)
{
  float l = in_out[0];
  float c = in_out[1];
  float h = in_out[2];
  float s;

  /* White and black: disambiguate saturation */
  if(l > 99.9999999 || l < 0.00000001)
    s = 0.0;
  else
    s = c / hsluv_max_chroma_for_lh(l, h) * 100.0;

  /* Grays: disambiguate hue */
  if (c < 0.00000001)
    h = 0.0;

  in_out[0] = h;
  in_out[1] = s;
  in_out[2] = l;
}

static inline void
hsluv_to_rec2020(const float hsl[], float rgb[])
{
  float tmp[] = {hsl[0]*360.0f, hsl[1]*100.0f, hsl[2]*100.0f};
  hsluv2lch(tmp);
  lch2luv(tmp);
  luv2xyz(tmp);
  const float xyz_to_rec2020[] = {
    1.71665119, -0.66668435,  0.01763986,
   -0.35567078,  1.61648124, -0.04277061,
   -0.25336628,  0.01576855,  0.94210312};
  rgb[0] = rgb[1] = rgb[2] = 0.0f;
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      rgb[k] += xyz_to_rec2020[3*i+k] * tmp[i];
  for(int i=0;i<3;i++) rgb[i] = CLAMP(rgb[i], 0.0f, 1.0f);
}

static inline void
rec2020_to_hsluv(const float rgb[], float hsl[])
{
  float tmp[3] = {0.0f};
  const float rec2020_to_xyz[] = {
    6.36958048e-01, 2.62700212e-01, 4.20575872e-11,
    1.44616904e-01, 6.77998072e-01, 2.80726931e-02,
    1.68880975e-01, 5.93017165e-02, 1.06098506e+00};
  for(int k=0;k<3;k++)
    for(int i=0;i<3;i++)
      tmp[k] += rec2020_to_xyz[3*i+k] * rgb[i];
  xyz2luv(tmp);
  luv2lch(tmp);
  lch2hsluv(tmp);
  hsl[0] = CLAMP(tmp[0]/360.0f, 0.0, 1.0f);
  hsl[1] = CLAMP(tmp[1]/100.0f, 0.0, 1.0f);
  hsl[2] = CLAMP(tmp[2]/100.0f, 0.0, 1.0f);
}

#if 0
void
hpluv2rgb(float h, float s, float l, float* pr, float* pg, float* pb)
{
    Triplet tmp = { h, s, l };

    hpluv2lch(&tmp);
    lch2luv(&tmp);
    luv2xyz(&tmp);
    xyz2rgb(&tmp);

    *pr = CLAMP(tmp.a, 0.0, 1.0);
    *pg = CLAMP(tmp.b, 0.0, 1.0);
    *pb = CLAMP(tmp.c, 0.0, 1.0);
}

int
rgb2hpluv(float r, float g, float b, float* ph, float* ps, float* pl)
{
    Triplet tmp = { r, g, b };

    rgb2xyz(&tmp);
    xyz2luv(&tmp);
    luv2lch(&tmp);
    lch2hpluv(&tmp);

    *ph = CLAMP(tmp.a, 0.0, 360.0);
    /* Do NOT clamp the saturation. Application may want to have an idea
     * how much off the valid range the given RGB color is. */
    *ps = tmp.b;
    *pl = CLAMP(tmp.c, 0.0, 100.0);

    return (0.0 <= tmp.b  &&  tmp.b <= 100.0) ? 0 : -1;
}
static float
dot_product(const Triplet* t1, const Triplet* t2)
{
    return (t1->a * t2->a + t1->b * t2->b + t1->c * t2->c);
}

/* Used for rgb conversions */
static float
from_linear(float c)
{
    if(c <= 0.0031308)
        return 12.92 * c;
    else
        return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

static float
to_linear(float c)
{
    if (c > 0.04045)
        return pow((c + 0.055) / 1.055, 2.4);
    else
        return c / 12.92;
}

static void
xyz2rgb(Triplet* in_out)
{
    float r = from_linear(dot_product(&m[0], in_out));
    float g = from_linear(dot_product(&m[1], in_out));
    float b = from_linear(dot_product(&m[2], in_out));
    in_out->a = r;
    in_out->b = g;
    in_out->c = b;
}

static void
rgb2xyz(Triplet* in_out)
{
    Triplet rgbl = { to_linear(in_out->a), to_linear(in_out->b), to_linear(in_out->c) };
    float x = dot_product(&m_inv[0], &rgbl);
    float y = dot_product(&m_inv[1], &rgbl);
    float z = dot_product(&m_inv[2], &rgbl);
    in_out->a = x;
    in_out->b = y;
    in_out->c = z;
}

/* for XYZ */
static const Triplet m_inv[3] = {
    {  0.41239079926595948129,  0.35758433938387796373,  0.18048078840183428751 },
    {  0.21263900587151035754,  0.71516867876775592746,  0.07219231536073371500 },
    {  0.01933081871559185069,  0.11919477979462598791,  0.95053215224966058086 }
};

static inline float
hsluv_dist_from_pole_squared(float x, float y)
{
  return x * x + y * y;
}

static inline float
hsluv_intersect_line_line(const float* line1, const float* line2)
{
  return (line1[1] - line2[1]) / (line2[0] - line1[0]);
}


static inline float
hsluv_max_safe_chroma_for_l(float l)
{
  float min_len_squared = FLT_MAX;
  float bounds[6][2];

  hsluv_get_bounds(l, bounds);
  for(int i = 0; i < 6; i++)
  {
    float m1 = bounds[i][0];
    float b1 = bounds[i][1];
    /* x where line intersects with perpendicular running though (0, 0) */
    float line2[] = { -1.0 / m1, 0.0 };
    float x = hsluv_intersect_line_line(bounds[i], line2);
    float distance = hsluv_dist_from_pole_squared(x, b1 + x * m1);
    if(distance < min_len_squared)
      min_len_squared = distance;
  }
  return sqrtf(min_len_squared);
}


static inline void
lch2hpluv(float * in_out)
{
  float l = in_out[0];
  float c = in_out[1];
  float h = in_out[2];
  float s;

  /* White and black: disambiguate saturation */
  if (l > 99.9999999 || l < 0.00000001)
    s = 0.0;
  else
    s = c / hsluv_max_safe_chroma_for_l(l) * 100.0;

  /* Grays: disambiguate hue */
  if (c < 0.00000001)
    h = 0.0;

  in_out[0] = h;
  in_out[1] = s;
  in_out[2] = l;
}

static inline void
hpluv2lch(float *in_out)
{
  float h = in_out[0];
  float s = in_out[1];
  float l = in_out[2];
  float c;

  /* White and black: disambiguate chroma */
  if(l > 99.9999999 || l < 0.00000001)
    c = 0.0;
  else
    c = hsluv_max_safe_chroma_for_l(l) / 100.0 * s;

  /* Grays: disambiguate hue */
  if (s < 0.00000001)
    h = 0.0;

  in_out[0] = l;
  in_out[1] = c;
  in_out[2] = h;
}


#endif
