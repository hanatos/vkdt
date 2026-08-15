#define COLOR_SPACE    COLOR_SPACE_P3_D65    // choose color space from below macro

#define COLOR_SPACE_sRGB      1
#define COLOR_SPACE_P3_D65    2
#define COLOR_SPACE_AdobeRGB  3
#define COLOR_SPACE_Rec2020   4
#define COLOR_SPACE_ACEScg    5

#if COLOR_SPACE == COLOR_SPACE_sRGB
// sRGB
#define COLOR_PRIMARIES ColorPrimaries(vec2(0.64f, 0.33f), vec2(0.3f, 0.6f), vec2(0.15f, 0.06f), vec2(0.3127f, 0.329f))
#define NEED_WHITE_POINT_CAT    0

#elif COLOR_SPACE == COLOR_SPACE_P3_D65
// P3 D65
#define COLOR_PRIMARIES ColorPrimaries(vec2(0.68f	, 0.32f		), vec2(0.265f	, 0.69f	), vec2(0.15f	, 0.06f		), vec2(0.3127f	, 0.329f	))
#define NEED_WHITE_POINT_CAT    0

#elif COLOR_SPACE == COLOR_SPACE_AdobeRGB
// AdobeRGB
#define COLOR_PRIMARIES ColorPrimaries(vec2(0.64f	, 0.33f		), vec2(0.21f	, 0.71f	), vec2(0.15f	, 0.06f		), vec2(0.3127f	, 0.329f	))
#define NEED_WHITE_POINT_CAT    0

#elif COLOR_SPACE == COLOR_SPACE_Rec2020
// Rec2020
#define	 COLOR_PRIMARIES ColorPrimaries(vec2(0.708f	, 0.292f	), vec2(0.17f	, 0.797f), vec2(0.131f	, 0.046f	), vec2(0.3127f	, 0.329f	))
#define NEED_WHITE_POINT_CAT    0

#elif COLOR_SPACE == COLOR_SPACE_ACEScg
// ACEScg
#define	 COLOR_PRIMARIES ColorPrimaries(vec2(0.713f	, 0.293f	), vec2(0.165f	, 0.830f), vec2(0.128f	, 0.044f	), vec2(0.32168f, 0.33767f	))
#define NEED_WHITE_POINT_CAT    1  // ACES use a white point near D60, which is different from Oklab D65, so need chromatic adaptation

#else
Unknown COLOR_SPACE, please define the COLOR_PRIMARIES and NEED_WHITE_POINT_CAT
#endif


#define OKLAB_WHITE_POINT   vec2(0.3127f, 0.329f)

#define mtx_XYZ_OklabLMS	transpose(mat3(	0.8189330101, 0.3618667424, -0.1288597137,\
                                            0.0329845436, 0.9293118715,  0.0361456387,\
                                            0.0482003018, 0.264366269 ,  0.6338517070))
#define mtx_OklabLMS_XYZ	transpose(mat3(	1.227013851103521026    ,  -0.5577999806518222383 ,  0.28125614896646780758, \
                                            -0.040580178423280593977,   1.1122568696168301049 ,  -0.071676678665601200577, \
                                            -0.076381284505706892869,  -0.42148197841801273055,  1.5861632204407947575 ))
                                    
#define PI     3.14159265359

// some of the function are borrowed from the following shader toy source code
// https://www.shadertoy.com/view/wdXBDS
// https://www.shadertoy.com/view/WlGyDG
// https://www.shadertoy.com/view/4lGSzV
// Bjorn's shadertoys: 
// - https://www.shadertoy.com/user/bjornornorn
// - https://www.shadertoy.com/view/7slBD2

float degree_to_radian(float deg)
{
    return deg * (PI /180.0);
}

float radian_to_degree(float rad)
{
    return rad * (180.0/PI);
}

float sdLine(in vec2 p,in vec2 a,in vec2 b)
{
    /// Project p onto the line ab, then return the distance from p` to p
    
	// Use a as 'origin'
    vec2 origin = a;    
    vec2 p2 = p-origin;
    vec2 b2 = b-origin;

    // Compute the dot product of vectors and divide to get the ratio
	float lineRatio = dot(p2, b2)/dot(b2, b2);

    // Clamp the ratio between 0.0 and 1.0
    lineRatio = clamp(lineRatio, 0., 1.);

    // Nearest point on AB
    vec2 pointOnLine = b2*lineRatio;

    // Return distance from line to point
    return 1. - step(0.5, length(p2 - pointOnLine));
}


#define RESOLUTION .3

// based on works of TimoKinnunen
// draws float <value> at iDrawCoord-<charCoord>
// works fine, but not for big decimals
float drawFloat(in vec2 charCoord, float value,
		float digits, float decimals)
{        
    // round to closest dp
    float dp_multiplier= pow(10., decimals);
    value= round(value * dp_multiplier) / dp_multiplier;
    
	charCoord *= RESOLUTION*RESOLUTION;
	float bits = 0.;
	if(charCoord.y < 0. || charCoord.y >= 1.5 || charCoord.x < step(-value,0.)) return bits;
	float digitIndex = digits - floor(charCoord.x)+ 1.;
	if(- digitIndex <= decimals) {
		float pow1 = pow(10., digitIndex);
		float absValue = abs(value);
		float pivot = max(absValue, 1.5) * 10.;
		if(pivot < pow1) bits = 1792.*float(value < 0. && pivot >= pow1 * .1);
		else if(digitIndex == 0.) bits = 2.*float(decimals > 0.);
		else {
        	value = digitIndex < 0. ? fract(absValue) : absValue * 10.;
            int x=int (mod(value / pow1, 10.));
			bits = x==0?480599.:x==1?139810.:x==2?476951.:x==3?476999.:x==4?350020.:x==5?464711.:x==6?464727.:x==7?476228.:x==8?481111.:x==9?481095.:0.;
		}
	}
	return floor(mod(bits / pow(2., floor(fract(charCoord.x) * 4.) + floor(charCoord.y * 4.) * 4.), 2.));
}

float drawCircle(vec2 pxCoord, vec2 circlePos, float circleRadius)
{
    return float(length(pxCoord - circlePos) <= circleRadius);
}

struct ColorPrimaries
{
	vec2	red;
	vec2	green;
	vec2	blue;
	vec2	white;
};

vec3	xyY_to_XYZ(vec3 color)
{
	float x= color.x;
	float y= color.y;
	float Y= color.z;
	return vec3(x*Y/y, Y, (1.0f - x - y)*Y/y);
}

vec3	xyY_to_XYZ(vec2 xy, float Y)
{
	return xyY_to_XYZ(vec3(xy, Y));
}

// http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
mat3	colorSpace_to_XYZ(ColorPrimaries colorPrimaries, float whitePointY)
{
	vec3 XYZr= xyY_to_XYZ(colorPrimaries.red	, 1.0f);
	vec3 XYZg= xyY_to_XYZ(colorPrimaries.green	, 1.0f);
	vec3 XYZb= xyY_to_XYZ(colorPrimaries.blue	, 1.0f);
	vec3 XYZw= xyY_to_XYZ(colorPrimaries.white	, whitePointY);

	vec3 S= inverse(mat3(XYZr, XYZg, XYZb)) * XYZw;
	return mat3(XYZr * S.x, XYZg * S.y, XYZb * S.z);
}

mat3	XYZ_to_colorSpace(ColorPrimaries colorPrimaries, float whitePointY)
{
    return inverse(colorSpace_to_XYZ(colorPrimaries, whitePointY));
}

// Chromatic Adaptation transformation between differnt white point,
// http://www.brucelindbloom.com/index.html?Eqn_ChromAdapt.html
mat3 mtx_CAT(vec2 chromaticityWhiteDest, vec2 chromaticityWhiteSrc)
{
	// cone response matrix BardFord
	const mat3 Ma= transpose(mat3(	 0.89510f,  0.26640f, -0.16140f,
                                    -0.75020f,  1.71350f,  0.03670f,
                                     0.03890f, -0.06850f,  1.02960f));
	const mat3 MaInv= inverse(Ma);
  
	vec3 src_XYZ = xyY_to_XYZ( chromaticityWhiteSrc	, 1.0f );
	vec3 des_XYZ = xyY_to_XYZ( chromaticityWhiteDest, 1.0f );

	vec3 src_coneResp = Ma * src_XYZ;
	vec3 des_coneResp = Ma * des_XYZ;

	mat3 vkMat= transpose(mat3( des_coneResp.x / src_coneResp.x , 0.0f								, 0.0f,
                                0.0f							, des_coneResp.y / src_coneResp.y	, 0.0 ,
                                0.0f							, 0.0								, des_coneResp.z / src_coneResp.z ));
	
	return MaInv * vkMat * Ma;
}
                                    
mat3 mtx_rgb_to_oklabLMS()
{
    mat3 mtx_RGB_XYZ= colorSpace_to_XYZ(COLOR_PRIMARIES, 1.0);
#if NEED_WHITE_POINT_CAT
    mat3 CAT= mtx_CAT(OKLAB_WHITE_POINT, COLOR_PRIMARIES.white);
    mat3 mtx_RGB_OklabLMS= mtx_XYZ_OklabLMS * CAT * mtx_RGB_XYZ;
#else
    mat3 mtx_RGB_OklabLMS= mtx_XYZ_OklabLMS * mtx_RGB_XYZ;
#endif
    return mtx_RGB_OklabLMS;
}
          
mat3 mtx_oklabLMS_to_rgb()
{
    mat3 mtx_XYZ_RGB= XYZ_to_colorSpace(COLOR_PRIMARIES, 1.0);
#if NEED_WHITE_POINT_CAT
    mat3 CAT= mtx_CAT(COLOR_PRIMARIES.white, OKLAB_WHITE_POINT);
    mat3 mtx_OklabLMS_RGB= mtx_XYZ_RGB * CAT * mtx_OklabLMS_XYZ;
#else
    mat3 mtx_OklabLMS_RGB= mtx_XYZ_RGB * mtx_OklabLMS_XYZ;
#endif
    return mtx_OklabLMS_RGB;
}

vec3 rgb_to_oklab(vec3 c) 
{
    mat3 mtx_RGB_OklabLMS= mtx_rgb_to_oklabLMS();
    
    float l = mtx_RGB_OklabLMS[0][0] * c.r + mtx_RGB_OklabLMS[1][0] * c.g + mtx_RGB_OklabLMS[2][0] * c.b;
    float m = mtx_RGB_OklabLMS[0][1] * c.r + mtx_RGB_OklabLMS[1][1] * c.g + mtx_RGB_OklabLMS[2][1] * c.b;
    float s = mtx_RGB_OklabLMS[0][2] * c.r + mtx_RGB_OklabLMS[1][2] * c.g + mtx_RGB_OklabLMS[2][2] * c.b;

    float l_ = pow(l, 1./3.);
    float m_ = pow(m, 1./3.);
    float s_ = pow(s, 1./3.);

    vec3 labResult;
    labResult.x = 0.2104542553f*l_ + 0.7936177850f*m_ - 0.0040720468f*s_;
    labResult.y = 1.9779984951f*l_ - 2.4285922050f*m_ + 0.4505937099f*s_;
    labResult.z = 0.0259040371f*l_ + 0.7827717662f*m_ - 0.8086757660f*s_;
    return labResult;
}

vec3 oklab_to_rgb(vec3 c) 
{
    float l_ = c.x + 0.3963377774f * c.y + 0.2158037573f * c.z;
    float m_ = c.x - 0.1055613458f * c.y - 0.0638541728f * c.z;
    float s_ = c.x - 0.0894841775f * c.y - 1.2914855480f * c.z;

    float l = l_*l_*l_;
    float m = m_*m_*m_;
    float s = s_*s_*s_;

    mat3 mtx_OklabLMS_RGB= mtx_oklabLMS_to_rgb();
    
    vec3 rgbResult;
    rgbResult.r = mtx_OklabLMS_RGB[0][0] *l + mtx_OklabLMS_RGB[1][0] *m + mtx_OklabLMS_RGB[2][0] *s;
    rgbResult.g = mtx_OklabLMS_RGB[0][1] *l + mtx_OklabLMS_RGB[1][1] *m + mtx_OklabLMS_RGB[2][1] *s;
    rgbResult.b = mtx_OklabLMS_RGB[0][2] *l + mtx_OklabLMS_RGB[1][2] *m + mtx_OklabLMS_RGB[2][2] *s;
    
    return rgbResult;
}

// asume cubic equation with a=1
float solve_cubic_halley(float x, float b, float c, float d)
{
	float f0 = x*x*x + b *x*x + c*x +d;
	float df1= 3.0f * x * x + 2.0f * b *x + c;
	float df2= 6.0f * x  + 2.0f * b;
	x= x - (f0 * df1)/(df1*df1 - 0.5f * f0*df2);
    
	return x;
}

// asume cubic equation with a=1
float solve_cubic_newton(float x, float b, float c, float d)
{
	float f0 = x*x*x + b *x*x + c*x +d;
	float df1= 3.0f * x * x + 2.0f * b *x + c;
	x= x - (f0/df1);
	return x;
}

float solve_cubic_numerical(float a, float b, float c, float d)
{
    b/=a;
    c/=a;
    d/=a;
    
    // 1 step of Halley + 1 step of Newton may be enough for small gamut like sRGB, AdobeRGB, DCI P3
    // larger gamut need more steps, especially at some blue hue
    float x= 0.4;  // initial guess
	x= solve_cubic_halley(x, b, c, d);
    x= solve_cubic_newton(x, b, c, d);
    return x;
}

// solve cubic eqt and only pick the 3rd root https://stackoverflow.com/questions/13328676/c-solving-cubic-equations
// and return only the minimum positive real root (which is our use case) 
float solve_cubic(float a, float b, float c, float d)
{
    b/=a;
    c/=a;
    d/=a;

    float q = (3.0/9.0)*c - ( (1.0/9.0) *b*b);
    float r = (-27.0/54.0)*d + b*((9.0/54.0)*c - (2.0/54.0)*(b*b));
    float disc = q*q*q + r*r;

    float term1 = b* (1.0/3.0);

    float x1_re= 0.0;
    float x2_re= 0.0;
    float x3_re= 0.0;
    float x;
    if (disc > 0.0)
    {
        // one root real, two are complex
        float s = r + sqrt(disc);
        s = sign(s) * pow(abs(s), 1.0/3.0);
        float t = r - sqrt(disc);
        t = sign(t) * pow(abs(t), 1.0/3.0);
        x1_re = -term1 + s + t;
        term1 += (s + t)/2.0;
        x2_re = -term1;
        x3_re = -term1;
        term1 = sqrt(3.0)*(-t + s)/2.0;
        
        x= x1_re; // only pick the real root for our use case
    }
    else
    {
        // The remaining options are all real
        q= -q;
        float dum1= q*q*q;
        dum1= acos( clamp(r/sqrt(dum1) , -1.0, 1.0) );
        float r13= 2.0 * sqrt(q);

        x1_re= -term1 + r13 * cos(dum1 * (1.0/3.0));
        x2_re= -term1 + r13 * cos(dum1 * (1.0/3.0) + (2.0*PI/3.0));
        x3_re= -term1 + r13 * cos(dum1 * (1.0/3.0) + (4.0*PI/3.0));

        x= 1.0f/max(1.0f/x1_re, max(1.0f/x2_re, 1.0f/x3_re)); // pick the minimum non negative for our use case
    }

    // perform Halley's method to fix floating point precision error at some hue (e.g. 232.59 degree)    
    x= solve_cubic_halley(x, b, c, d);
	return x;
}

// Finds the maximum saturation possible for a given hue that fits in given RGB space
// Saturation here is defined as S = C/L
// a and b must be normalized so a^2 + b^2 == 1
float compute_max_saturation(float a, float b, vec2 r_dir, vec2 g_dir, vec2 b_dir)
{
    // solve the max saturation value analytically
    float A= 0.3963377774;
    float B= 0.2158037573;
    float C= -0.1055613458;
    float D= -0.0638541728;
    float E= -0.0894841775;
    float F= -1.2914855480;

    vec2 ab= vec2(a, b);
    
    float Aa_Bb  = A * a + B * b;
    float Aa_Bb_2= Aa_Bb * Aa_Bb;
    float Aa_Bb_3= Aa_Bb * Aa_Bb_2;

    float Ca_Db= C * a + D * b;
    float Ca_Db_2= Ca_Db * Ca_Db;
    float Ca_Db_3= Ca_Db * Ca_Db_2;

    float Ea_Fb= E * a + F * b;
    float Ea_Fb_2= Ea_Fb * Ea_Fb;
    float Ea_Fb_3= Ea_Fb * Ea_Fb_2;
    
    float kl, km, ks;
    mat3 mtx_OklabLMS_RGB= mtx_oklabLMS_to_rgb();
    if (dot(r_dir, ab)> 1.)
    {
        // Red component
        kl= mtx_OklabLMS_RGB[0][0];
        km= mtx_OklabLMS_RGB[1][0];
        ks= mtx_OklabLMS_RGB[2][0];
    }
    else if (dot(g_dir, ab) > 1.)
    {
        // Green component
        kl= mtx_OklabLMS_RGB[0][1];
        km= mtx_OklabLMS_RGB[1][1];
        ks= mtx_OklabLMS_RGB[2][1];
    }
    else
    {
        // Blue component
        kl= mtx_OklabLMS_RGB[0][2];
        km= mtx_OklabLMS_RGB[1][2];
        ks= mtx_OklabLMS_RGB[2][2];
    }

    float coef3=        kl * Aa_Bb_3 + km * Ca_Db_3  + ks * Ea_Fb_3 ;
    float coef2= 3.0 * (kl * Aa_Bb_2 + km * Ca_Db_2  + ks * Ea_Fb_2);
    float coef1= 3.0 * (kl * Aa_Bb   + km * Ca_Db    + ks * Ea_Fb  );
    float coef0=        kl           + km            + ks;
    float S= solve_cubic(coef3, coef2, coef1, coef0);
    //float S= solve_cubic_numerical(coef3, coef2, coef1, coef0);
    
    return S;
}

// finds L_cusp and C_cusp for a given hue
// a and b must be normalized so a^2 + b^2 == 1
#define LC vec2
LC find_cusp(float a, float b, vec2 r_dir, vec2 g_dir, vec2 b_dir)
{
	// First, find the maximum saturation (saturation S = C/L)
	float S_cusp = compute_max_saturation(a, b, r_dir, g_dir, b_dir);

	// Convert to linear RGB to find the first point where at least one of r,g or b >= 1:
	vec3 rgb_at_max = oklab_to_rgb(vec3( 1., S_cusp * a, S_cusp * b ));
	float L_cusp = pow(1.f / max(max(rgb_at_max.r, rgb_at_max.g), rgb_at_max.b), 1./3.);
	float C_cusp = L_cusp * S_cusp;

	return LC( L_cusp , C_cusp );
}

// Finds intersection of the line defined by 
// L = L0 * (1 - t) + t * L1;
// C = t * C1;
// a and b must be normalized so a^2 + b^2 == 1
float find_gamut_intersection(float a, float b, float L1, float C1, float L0, vec2 r_dir, vec2 g_dir, vec2 b_dir)
{   
    float cosh= a;
    float sinh= b; // save the value here as b is redefined when using Halley's method...
    vec2 ab= vec2(a, b);

	// Find the cusp of the gamut triangle
	LC cusp = find_cusp(a, b, r_dir, g_dir, b_dir);

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
			{
				float L = L0 * (1.f - t) + t * L1;
				float C = t * C1;

				float l_ = L + C * k_l;
				float m_ = L + C * k_m;
				float s_ = L + C * k_s;

				float l = l_ * l_ * l_;
				float m = m_ * m_ * m_;
				float s = s_ * s_ * s_;

				float ldt = 3. * l_dt * l_ * l_;
				float mdt = 3. * m_dt * m_ * m_;
				float sdt = 3. * s_dt * s_ * s_;

				float ldt2 = 6. * l_dt * l_dt * l_;
				float mdt2 = 6. * m_dt * m_dt * m_;
				float sdt2 = 6. * s_dt * s_dt * s_;

                mat3 mtx_OklabLMS_RGB= mtx_oklabLMS_to_rgb();

				float r = mtx_OklabLMS_RGB[0][0] * l    + mtx_OklabLMS_RGB[1][0] * m    + mtx_OklabLMS_RGB[2][0] * s - 1.;
				float r1= mtx_OklabLMS_RGB[0][0] * ldt  + mtx_OklabLMS_RGB[1][0] * mdt  + mtx_OklabLMS_RGB[2][0] * sdt;
				float r2= mtx_OklabLMS_RGB[0][0] * ldt2 + mtx_OklabLMS_RGB[1][0] * mdt2 + mtx_OklabLMS_RGB[2][0] * sdt2;

				float u_r = r1 / (r1 * r1 - 0.5f * r * r2);
				float t_r = -r * u_r;

				float g  = mtx_OklabLMS_RGB[0][1] * l    + mtx_OklabLMS_RGB[1][1] * m    + mtx_OklabLMS_RGB[2][1] * s - 1.;
				float g1 = mtx_OklabLMS_RGB[0][1] * ldt  + mtx_OklabLMS_RGB[1][1] * mdt  + mtx_OklabLMS_RGB[2][1] * sdt;
				float g2 = mtx_OklabLMS_RGB[0][1] * ldt2 + mtx_OklabLMS_RGB[1][1] * mdt2 + mtx_OklabLMS_RGB[2][1] * sdt2;

				float u_g = g1 / (g1 * g1 - 0.5f * g * g2);
				float t_g = -g * u_g;

				float b  = mtx_OklabLMS_RGB[0][2] * l    + mtx_OklabLMS_RGB[1][2] * m    + mtx_OklabLMS_RGB[2][2] * s - 1.;
				float b1 = mtx_OklabLMS_RGB[0][2] * ldt  + mtx_OklabLMS_RGB[1][2] * mdt  + mtx_OklabLMS_RGB[2][2] * sdt;
				float b2 = mtx_OklabLMS_RGB[0][2] * ldt2 + mtx_OklabLMS_RGB[1][2] * mdt2 + mtx_OklabLMS_RGB[2][2] * sdt2;

				float u_b = b1 / (b1 * b1 - 0.5f * b * b2);
				float t_b = -b * u_b;

                float FLT_MAX= 3.402823466e+38;
#if 0 // check all 3 clipping lines
				t_r = u_r >= 0.f ? t_r : FLT_MAX;
				t_g = u_g >= 0.f ? t_g : FLT_MAX;
				t_b = u_b >= 0.f ? t_b : FLT_MAX;
				t += min(t_r, min(t_g, t_b));
#else // only check 2 upper clipping lines is enough
                if (dot(r_dir, ab)> 1.)
                {
                    // Red component
                    t_g = u_g >= 0.f ? t_g : FLT_MAX;
                    t_b = u_b >= 0.f ? t_b : FLT_MAX;
                    t += min(t_g, t_b);
                }
                else if (dot(g_dir, ab) > 1.)
                {
                    // Green component
                    t_r = u_r >= 0.f ? t_r : FLT_MAX;
                    t_b = u_b >= 0.f ? t_b : FLT_MAX;
                    t += min(t_r, t_b);
                }
                else
                {
                    // Blue component
                    t_r = u_r >= 0.f ? t_r : FLT_MAX;
                    t_g = u_g >= 0.f ? t_g : FLT_MAX;
                    t += min(t_r, t_g);
                }
#endif
			}
		}
	}

	return t;
}