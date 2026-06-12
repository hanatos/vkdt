// https://bottosson.github.io/misc/ok_color.h
/*
 * Copyright (c) 2021 Björn Ottosson
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */


// #include "common.glsl"
#include "common-precomp.glsl"

vec2 gamut_clip_preserve_chroma(float a_, float b_, float L1, float C1)
{
  float L0 = clamp(L1, 0, 1);

  float t = find_gamut_intersection(a_, b_, L1, C1, L0);
  float L_clipped = L0 * (1 - t) + t * L1;
  float C_clipped = t * C1;

  return vec2(L_clipped, C_clipped);
}

vec3 gamut_clip_preserve_chroma_lab(vec3 lab)
{

	float L = lab.x;
	float eps = 0.00001f;
	float C = max(eps, sqrt(lab.y * lab.y + lab.z * lab.z));
	float a_ = lab.y / C;
	float b_ = lab.z / C;

  vec2 lc_clipped = gamut_clip_preserve_chroma(a_, b_, L, C);
  float L_clipped = lc_clipped.x;
  float C_clipped = lc_clipped.y;

  return vec3(L_clipped, C_clipped * a_, C_clipped * b_);
}

vec3 gamut_clip_preserve_chroma(vec3 rgb)
{
	if (rgb.r < 1 && rgb.g < 1 && rgb.b < 1 && rgb.r > 0 && rgb.g > 0 && rgb.b > 0)
		return rgb;

	vec3 lab = rec2020_to_oklab(rgb);
  vec3 lab_gamut = gamut_clip_preserve_chroma_lab(lab);
  return oklab_to_rec2020(lab_gamut);
}


vec2 gamut_clip_project_to_L0_0_5(float a_, float b_, float L1, float C1)
{
  float L0 = 0.5;

  float t = find_gamut_intersection(a_, b_, L1, C1, L0);
  float L_clipped = L0 * (1 - t) + t * L1;
  float C_clipped = t * C1;

  return vec2(L_clipped, C_clipped);
}

vec3 gamut_clip_project_to_L0_0_5_lab(vec3 lab)
{

	float L = lab.x;
	float eps = 0.00001f;
	float C = max(eps, sqrt(lab.y * lab.y + lab.z * lab.z));
	float a_ = lab.y / C;  // aka cos(h)
	float b_ = lab.z / C;  // aka sin(h)

	vec2 lc_clipped = gamut_clip_project_to_L0_0_5(a_, b_, L, C);
  float L_clipped = lc_clipped.x;
  float C_clipped = lc_clipped.y;

  return vec3(L_clipped, C_clipped * a_, C_clipped * b_);
}

vec3 gamut_clip_project_to_L0_0_5(vec3 rgb)
{
	if (rgb.r < 1 && rgb.g < 1 && rgb.b < 1 && rgb.r > 0 && rgb.g > 0 && rgb.b > 0)
		return rgb;

	vec3 lab = rec2020_to_oklab(rgb);
  vec3 lab_gamut = gamut_clip_project_to_L0_0_5_lab(lab);
  return oklab_to_rec2020(lab_gamut);
}

vec2 gamut_clip_project_to_L_cusp(float a_, float b_, float L1, float C1)
{
  // The cusp is computed here and in find_gamut_intersection, an optimized solution would only compute it once.
  vec2 cusp = find_cusp(a_, b_);

  float L0 = cusp.x;

  float t = find_gamut_intersection(a_, b_, L1, C1, L0);
  float L_clipped = L0 * (1 - t) + t * L1;
  float C_clipped = t * C1;

  return vec2(L_clipped, C_clipped);
}

vec3 gamut_clip_project_to_L_cusp_lab(vec3 lab)
{
	float L = lab.x;
	float eps = 0.00001f;
	float C = max(eps, sqrt(lab.y * lab.y + lab.z * lab.z));
	float a_ = lab.y / C;
	float b_ = lab.z / C;

	vec2 lc_clipped = gamut_clip_project_to_L_cusp(a_, b_, L, C);
  float L_clipped = lc_clipped.x;
  float C_clipped = lc_clipped.y;

  return vec3(L_clipped, C_clipped * a_, C_clipped * b_);
}
vec3 gamut_clip_project_to_L_cusp(vec3 rgb)
{
	if (rgb.r < 1 && rgb.g < 1 && rgb.b < 1 && rgb.r > 0 && rgb.g > 0 && rgb.b > 0)
		return rgb;

	vec3 lab = rec2020_to_oklab(rgb);
  vec3 lab_gamut = gamut_clip_project_to_L_cusp_lab(lab);
  return oklab_to_rec2020(lab_gamut);
}

vec2 gamut_clip_adaptive_L0_0_5(float a_, float b_, float L1, float C1, float alpha)
{
  float Ld = L1 - 0.5f;
  float e1 = 0.5f + abs(Ld) + alpha * C1;
  float L0 = 0.5f*(1.f + sign(Ld)*(e1 - sqrt(e1*e1 - 2.f *abs(Ld))));

  float t = find_gamut_intersection(a_, b_, L1, C1, L0);
  float L_clipped = L0 * (1.f - t) + t * L1;
  float C_clipped = t * C1;
  return vec2(L_clipped, C_clipped);
}

vec3 gamut_clip_adaptive_L0_0_5_lab(vec3 lab, float alpha)
{
	float L = lab.x;
	float eps = 0.00001f;
	float C = max(eps, sqrt(lab.y * lab.y + lab.z * lab.z));
	float a_ = lab.y / C;
	float b_ = lab.z / C;

	vec2 lc_clipped = gamut_clip_adaptive_L0_0_5(a_, b_, L, C, alpha);
  float L_clipped = lc_clipped.x;
  float C_clipped = lc_clipped.y;
  return vec3(L_clipped, C_clipped * a_, C_clipped * b_);
}

vec3 gamut_clip_adaptive_L0_0_5(vec3 rgb, float alpha)
{
	if (rgb.r < 1 && rgb.g < 1 && rgb.b < 1 && rgb.r > 0 && rgb.g > 0 && rgb.b > 0)
		return rgb;

	vec3 lab = rec2020_to_oklab(rgb);
  vec3 lab_gamut = gamut_clip_adaptive_L0_0_5_lab(lab, alpha);
  return oklab_to_rec2020(lab_gamut);
}

vec2 gamut_clip_adaptive_L0_L_cusp(float a_, float b_, float L1, float C1, float alpha)
{
  // The cusp is computed here and in find_gamut_intersection, an optimized solution would only compute it once.
  vec2 cusp = find_cusp(a_, b_);

  float Ld = L1 - cusp.x;
  float k = 2.f * (Ld > 0 ? 1.f - cusp.x : cusp.x);

  float e1 = 0.5f*k + abs(Ld) + alpha * C1/k;
  float L0 = cusp.x + 0.5f * (sign(Ld) * (e1 - sqrt(e1 * e1 - 2.f * k * abs(Ld))));

  float t = find_gamut_intersection(a_, b_, L1, C1, L0);
  float L_clipped = L0 * (1.f - t) + t * L1;
  float C_clipped = t * C1;

  return vec2(L_clipped, C_clipped);
}

vec3 gamut_clip_adaptive_L0_L_cusp_lab(vec3 lab, float alpha)
{
	float L = lab.x;
	float eps = 0.00001f;
	float C = max(eps, sqrt(lab.y * lab.y + lab.z * lab.z));
	float a_ = lab.y / C;
	float b_ = lab.z / C;

	vec2 lc_clipped = gamut_clip_adaptive_L0_L_cusp(a_, b_, L, C, alpha);
  float L_clipped = lc_clipped.x;
  float C_clipped = lc_clipped.y;

  return vec3(L_clipped, C_clipped * a_, C_clipped * b_);
}

vec3 gamut_clip_adaptive_L0_L_cusp(vec3 rgb, float alpha)
{
	if (rgb.r < 1 && rgb.g < 1 && rgb.b < 1 && rgb.r > 0 && rgb.g > 0 && rgb.b > 0)
		return rgb;

	vec3 lab = rec2020_to_oklab(rgb);
  vec3 lab_gamut = gamut_clip_adaptive_L0_L_cusp_lab(lab, alpha);
  return oklab_to_rec2020(lab_gamut);
}