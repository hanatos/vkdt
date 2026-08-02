// common functions with precomputed coefficients for rec2020 gamut
#include "colourspaces.glsl"

const float FLT_MAX = 3.402823466e+38;

float fixedpow(float a, float x)
{
  return pow(abs(a), x) * sign(a);
}

float cbrt(float a)
{
  return fixedpow(a, 0.3333333333);
}

// h in radians
vec3 oklab_LCh_to_Lab(float L, float C, float h) 
{
  vec3 lab;
  lab.x = L;
  lab.y = C * cos(h);
  lab.z = C * sin(h);
  return lab;
}

// h in radians
vec3 oklab_Lab_to_LCh(float L, float a, float b) 
{
  float C = sqrt(a * a + b * b);
  float h = atan(b, a);
  return vec3(L, C, h);
}


// Finds the maximum saturation possible for a given hue that fits in sRGB/rec2020 gamut
// Saturation here is defined as S = C/L
// a and b must be normalized so a^2 + b^2 == 1
float compute_max_saturation(float a, float b)
{
  // Max saturation will be when one of r, g or b goes below zero.

  // Select different coefficients depending on which component goes below zero first
  float k0, k1, k2, k3, k4, wl, wm, ws;

	// the k0 are tuned slightly from those Bjorn provided, but I think they still need some work

  // if (-1.88170328f * a - 0.80936493f * b > 1) // srgb
  if (-1.36829327f * a - 0.46690566f * b > 1) // rec2020
  {
    // Red component
    // k0 = +1.19086277f; k1 = +1.76576728f; k2 = +0.59662641f; k3 = +0.75515197f; k4 = +0.56771245f;
    k0 = +1.56640423f; k1 = +1.77323853f; k2 = +0.58667604f; k3 = +0.73320572f; k4 = +0.58300929f;
    // wl = +4.0767416621f; wm = -3.3077115913f; ws = +0.2309699292f; // srgb
    wl = +2.14014041f; wm = -1.24635595f; ws = +0.10643173f; // rec2020
  }
  // else if (1.81444104f * a - 1.19445276f * b > 1) // srgb
  else if (2.01082106f * a - 2.03699421f * b > 1) // rec2020
  {
    // Green component
    // k0 = +0.73956515f; k1 = -0.45954404f; k2 = +0.08285427f; k3 = +0.12541070f; k4 = +0.14503204f;
    k0 = +0.80978699f; k1 = -0.42456418f; k2 = +0.08285427f; k3 = +0.14414336f; k4 = +0.1443485f;
    // wl = -1.2684380046f; wm = +2.6097574011f; ws = -0.3413193965f; // srgb
    wl = -0.88483245f; wm = +2.16317272f; ws = -0.27836159f; // rec2020
  }
  else
  {
    // Blue component
    // k0 = +1.35733652f; k1 = -0.00915799f; k2 = -1.15130210f; k3 = -0.50559606f; k4 = +0.00692167f;
    k0 = +1.41503345f; k1 = -0.02049584f; k2 = -1.15438812f; k3 = -0.49243865f; k4 = +0.0081505f;
    // wl = -0.0041960863f; wm = -0.7034186147f; ws = +1.7076147010f; // srgb
    wl = -0.04857906f; wm = -0.45449091f; ws = +1.50235629f; // rec2020
  }

  // Approximate max saturation using a polynomial:
  float S = k0 + k1 * a + k2 * b + k3 * a * a + k4 * a * b;

  // Do one step Halley's method to get closer
  // this gives an error less than 10e6, except for some blue hues where the dS/dh is close to infinite
  // this should be sufficient for most applications, otherwise do two/three steps 

  float k_l = +0.3963377774f * a + 0.2158037573f * b;
  float k_m = -0.1055613458f * a - 0.0638541728f * b;
  float k_s = -0.0894841775f * a - 1.2914855480f * b;

  int num_iterations = 3;
  for (int i = 0; i < num_iterations; i++)
  {
    float l_ = 1.f + S * k_l;
    float m_ = 1.f + S * k_m;
    float s_ = 1.f + S * k_s;

    float l = l_ * l_ * l_;
    float m = m_ * m_ * m_;
    float s = s_ * s_ * s_;

    float l_dS = 3.f * k_l * l_ * l_;
    float m_dS = 3.f * k_m * m_ * m_;
    float s_dS = 3.f * k_s * s_ * s_;

    float l_dS2 = 6.f * k_l * k_l * l_;
    float m_dS2 = 6.f * k_m * k_m * m_;
    float s_dS2 = 6.f * k_s * k_s * s_;

    float f  = wl * l     + wm * m     + ws * s;
    float f1 = wl * l_dS  + wm * m_dS  + ws * s_dS;
    float f2 = wl * l_dS2 + wm * m_dS2 + ws * s_dS2;

    S = S - f * f1 / (f1*f1 - 0.5f * f * f2);
  }

  return S;
}


// finds L_cusp and C_cusp for a given hue
// a and b must be normalized so a^2 + b^2 == 1
vec2 find_cusp(float a, float b)
{
	// First, find the maximum saturation (saturation S = C/L)
	float S_cusp = compute_max_saturation(a, b);

	// Convert to linear sRGB to find the first point where at least one of r,g or b >= 1:
  vec3 lab = vec3(1, S_cusp * a, S_cusp * b);
  vec3 rgb_at_max = oklab_to_rec2020(lab);
  float L_cusp = cbrt(1.f / max(max(rgb_at_max.r, rgb_at_max.g), rgb_at_max.b));
  float C_cusp = L_cusp * S_cusp;

  return vec2(L_cusp, C_cusp);
}


// Finds intersection of the line defined by 
// L = L0 * (1 - t) + t * L1;
// C = t * C1;
// a and b must be normalized so a^2 + b^2 == 1
float find_gamut_intersection(float a, float b, float L1, float C1, float L0)
{
	// Find the cusp of the gamut triangle
	vec2 cusp = find_cusp(a, b);

	// Find the intersection for upper and lower half seprately
	float t;
	if (((L1 - L0) * cusp.y - (cusp.x - L0) * C1) <= 0.f)
	{
		// Lower half
		t = cusp.y * L0 / (C1 * cusp.x + cusp.y * (L0 - L1));
	}
	else
	{
		// Upper half

		// First intersect with triangle
		t = cusp.y * (L0 - 1.f) / (C1 * (cusp.x - 1.f) + cusp.y * (L0 - L1));

		// Then one step Halley's method
		{
			float dL = L1 - L0;
			float dC = C1;

			float k_l = +0.3963377774f * a + 0.2158037573f * b;
			float k_m = -0.1055613458f * a - 0.0638541728f * b;
			float k_s = -0.0894841775f * a - 1.2914855480f * b;

			float l_dt = dL + dC * k_l;
			float m_dt = dL + dC * k_m;
			float s_dt = dL + dC * k_s;

			
			// If higher accuracy is required, 2 or 3 iterations of the following block can be used:
			int num_iterations = 3;
			for (int i = 0; i < num_iterations; i++)
			{
				float L = L0 * (1.f - t) + t * L1;
				float C = t * C1;

				float l_ = L + C * k_l;
				float m_ = L + C * k_m;
				float s_ = L + C * k_s;

				float l = l_ * l_ * l_;
				float m = m_ * m_ * m_;
				float s = s_ * s_ * s_;

				float ldt = 3 * l_dt * l_ * l_;
				float mdt = 3 * m_dt * m_ * m_;
				float sdt = 3 * s_dt * s_ * s_;

				float ldt2 = 6 * l_dt * l_dt * l_;
				float mdt2 = 6 * m_dt * m_dt * m_;
				float sdt2 = 6 * s_dt * s_dt * s_;

				// rec2020
				float r = 2.14014041f * l - 1.24635595f * m + 0.10643173f * s - 1;
				float r1 = 2.14014041f * ldt - 1.24635595f * mdt + 0.10643173f * sdt;
				float r2 = 2.14014041f * ldt2 - 1.24635595f * mdt2 + 0.10643173f * sdt2;

				float u_r = r1 / (r1 * r1 - 0.5f * r * r2);
				float t_r = -r * u_r;

				// rec2020
				float g = -0.88483245f * l + 2.16317272f * m - 0.27836159f * s - 1;
				float g1 = -0.88483245f * ldt + 2.16317272f * mdt - 0.27836159f * sdt;
				float g2 = -0.88483245f * ldt2 + 2.16317272f * mdt2 - 0.27836159f * sdt2;

				float u_g = g1 / (g1 * g1 - 0.5f * g * g2);
				float t_g = -g * u_g;

				// rec2020
				float b = -0.04857906f * l - 0.45449091f * m + 1.50235629f * s - 1;
				float b1 = -0.04857906f * ldt - 0.45449091f * mdt + 1.50235629f * sdt;
				float b2 = -0.04857906f * ldt2 - 0.45449091f * mdt2 + 1.50235629f * sdt2;

				float u_b = b1 / (b1 * b1 - 0.5f * b * b2);
				float t_b = -b * u_b;

				t_r = u_r >= 0.f ? t_r : FLT_MAX;
				t_g = u_g >= 0.f ? t_g : FLT_MAX;
				t_b = u_b >= 0.f ? t_b : FLT_MAX;

				t += min(t_r, min(t_g, t_b));
			}
		}
	}

	return t;
}