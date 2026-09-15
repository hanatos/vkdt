#define HUE_MODE                               2     // 0= use HUE defined below
                                                     // 1= use mouse x position as HUE
                                                     // 2= slowly changing HUE based of time
#define HUE                                    108.74  // when HUE_MODE == 0, use this defined value for hue, in degree
#define HUE_CHANGE_SPEED                       30.0  // when HUE_MODE == 2, the speed of HUE change
#define DISPLAY_SATURATION_MODE                0     // set to 1 to display grey scale saturation
#define IS_DISPLAY_GAMUT_CUSP_POINT            1
#define IS_DISPLAY_GAMUT_CLIP_LINE             1
#define IS_DISPLAY_RGB_COLOR_NEAR_ZERO_LINE    1
#define IS_DISPLAY_RGB_COLOR_NEAR_ONE_LINE     1
#define IS_DISPLAY_TEXT_HUE                    1
#define IS_DISPLAY_TEXT_MOUSE_RGB_SAT          1
#define IS_DISPLAY_TEXT_PROJECTED_RGB_SAT      1
#define IS_DISPLAY_TEXT_CUSP_RGB_SAT           1
#define IS_DISPLAY_OUT_OF_GAMUT_COLOR          0
#define GAMUT_CLIP_L0                          0.5   // the lightness value projected to
#define DRAW_SCALE_CL_MIN                      vec2(0.0, -0.05)
#define DRAW_SCALE_CL_MAX                      vec2(0.5,  1.25)

vec2 pxCoord_to_CL(vec2 pxPos)
{
    vec2 CL_range= DRAW_SCALE_CL_MAX - DRAW_SCALE_CL_MIN;
    return (pxPos / iResolution.xy) * CL_range + DRAW_SCALE_CL_MIN;
}

vec2 CL_to_pxCoord(vec2 CL)
{
    vec2 CL_range= DRAW_SCALE_CL_MAX - DRAW_SCALE_CL_MIN;
    return ((CL - DRAW_SCALE_CL_MIN)/CL_range)* iResolution.xy;
}

float drawCircleAtLC(vec2 pxCoord, LC circlePosLC, float circleRadius)
{
    return drawCircle(pxCoord, CL_to_pxCoord(circlePosLC.yx), circleRadius);
}

float OETF_rec709_f(float val)
{
	return val < 0.018 ? val * 4.5: 1.099 * pow(val, 0.45) - 0.099;
}

vec3 OETF_rec709(vec3 val)
{
	return vec3(OETF_rec709_f(val.r), OETF_rec709_f(val.g), OETF_rec709_f(val.b));
}

// h in radian
vec3 oklab_LCh_to_Lab(float L, float C, float h) 
{
    vec3 Lab;
    Lab.x = L;
    Lab.y = C * cos(h);
    Lab.z = C * sin(h);
    return Lab;
}

// h in radian
vec3 oklab_pxCoord_to_Lab(vec2 pxPos, float h) 
{
    vec2  CL= pxCoord_to_CL(pxPos);
    return oklab_LCh_to_Lab(CL.y, CL.x, h);
}

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // draw gamut at specific hue
#if HUE_MODE == 0
    float hue = degree_to_radian(HUE);
#elif HUE_MODE == 1
    float hue = degree_to_radian(iMouse.x/iResolution.x * 360.);
#else
    float hue = degree_to_radian(mod(iTime * HUE_CHANGE_SPEED, 360.));
#endif
    float cos_h= cos(hue);
    float sin_h= sin(hue);
    
    // convert pixel position to lightness and chroma
    vec2  CL_px   = pxCoord_to_CL(fragCoord);
    float sat_px  = CL_px.x / CL_px.y;
    vec3  oklab_px= oklab_LCh_to_Lab(CL_px.y, CL_px.x, hue);
    
    // compute rgb_dir for a given color space
    vec3 r_lab = rgb_to_oklab(vec3(1,0,0));
    vec3 g_lab = rgb_to_oklab(vec3(0,1,0));
    vec3 b_lab = rgb_to_oklab(vec3(0,0,1));

    float r_h = atan(r_lab.z, r_lab.y);
    float g_h = atan(g_lab.z, g_lab.y);
    float b_h = atan(b_lab.z, b_lab.y);

    vec2 r_dir = 0.5*vec2(cos(b_h)+cos(g_h), sin(b_h)+sin(g_h));
    vec2 g_dir = 0.5*vec2(cos(b_h)+cos(r_h), sin(b_h)+sin(r_h));
    vec2 b_dir = 0.5*vec2(cos(r_h)+cos(g_h), sin(r_h)+sin(g_h));
    
    r_dir /= dot(r_dir, r_dir);
    g_dir /= dot(g_dir, g_dir);
    b_dir /= dot(b_dir, b_dir);
    
    // find gamut cusp
    LC    cusp    = find_cusp(cos_h, sin_h, r_dir, g_dir, b_dir);
    
    // project out of gamut point back to gamut boundary
    LC    ProjectPtFrom= pxCoord_to_CL(iMouse.xy).yx;
    LC    ProjectPtTo  = LC(GAMUT_CLIP_L0, 0);  // can also use cusp.x if want to project to lightness of cusp
	float t            = find_gamut_intersection(cos_h, sin_h, ProjectPtFrom.x, ProjectPtFrom.y, ProjectPtTo.x, r_dir, g_dir, b_dir);
	float L_clipped    = ProjectPtTo.x * (1. - t) + t * ProjectPtFrom.x;
	float C_clipped    = t * ProjectPtFrom.y;
    LC    LC_clipped   = LC(L_clipped, C_clipped);
    vec3  projectedRGB = oklab_to_rgb( vec3(L_clipped, C_clipped * cos_h, C_clipped * sin_h));
    
    // convert pixel color to RGB space
    vec3 lin_RGB       = oklab_to_rgb(oklab_px);
    vec3 clipped_RGB   = clamp(lin_RGB, 0., 1.);
    
    // compute output color
    float epsilon0= 0.0025 * pow(clamp(CL_px.y, 0., 1.), 1.5);  // scale down epsilon when close to zero
    bool isAnyChannelCloseToZeroR=abs(lin_RGB.r) < epsilon0;
    bool isAnyChannelCloseToZeroG=abs(lin_RGB.g) < epsilon0;
    bool isAnyChannelCloseToZeroB=abs(lin_RGB.b) < epsilon0;
    
    float epsilon1= 0.005;
    bool isAnyChannelCloseToOneR= abs(lin_RGB.r-1.) < epsilon1;
    bool isAnyChannelCloseToOneG= abs(lin_RGB.g-1.) < epsilon1;
    bool isAnyChannelCloseToOneB= abs(lin_RGB.b-1.) < epsilon1;
    
    bool isOutOfGamut=  lin_RGB.r != clipped_RGB.r ||
                        lin_RGB.g != clipped_RGB.g ||
                        lin_RGB.b != clipped_RGB.b ;
#if IS_DISPLAY_RGB_COLOR_NEAR_ZERO_LINE
    if (isAnyChannelCloseToZeroR)
        fragColor = vec4(.4,0.,0.,1.);
    else if (isAnyChannelCloseToZeroG)
        fragColor = vec4(0.,.35,0.,1.);
    else if (isAnyChannelCloseToZeroB)
        fragColor = vec4(0.,0.,.45,1.);
    else
#endif
#if IS_DISPLAY_RGB_COLOR_NEAR_ONE_LINE
    if (isAnyChannelCloseToOneR)
        fragColor = vec4(1.,0.,0.,1.);
    else if (isAnyChannelCloseToOneG)
        fragColor = vec4(0.,1.,0.,1.);
    else if (isAnyChannelCloseToOneB)
        fragColor = vec4(0.,0.,1.,1.);
    else
#endif
#if !IS_DISPLAY_OUT_OF_GAMUT_COLOR
    if(isOutOfGamut)  // out of gamut
        fragColor = vec4(0.5,0.5,0.5,1.);
    else
#endif
    {
        // within gamut
#if DISPLAY_SATURATION_MODE
        
        fragColor = vec4(sat_px, sat_px, sat_px, 1.);
#else
        // as we are interested in gamut intersection, but not the display color, 
        // so just use the rec709 OETF and don't bother to use the actual display OETF
        // for simplicity
        fragColor = vec4(OETF_rec709(clipped_RGB), 1.);
#endif
    }

    // draw current HUE in upper right corner
#if IS_DISPLAY_TEXT_HUE
    fragColor+=drawFloat(fragCoord-vec2(iResolution.x - 80., iResolution.y - 20.), radian_to_degree(hue), 3.0,2.0);
#endif

    // draw RGB + saturation at mouse position at the lowest line
#if IS_DISPLAY_TEXT_MOUSE_RGB_SAT
    vec2 oklab_mouse_CL= pxCoord_to_CL(iMouse.xy);
    vec3 oklab_mouse   = oklab_LCh_to_Lab(oklab_mouse_CL.y, oklab_mouse_CL.x, hue);
    vec3 lin_RGB_mouse= oklab_to_rgb(oklab_mouse);
    float sat_mouse= oklab_mouse_CL.x / oklab_mouse_CL.y;
    fragColor+=drawFloat(fragCoord-vec2(000.0, 0.0), lin_RGB_mouse.r, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(100.0, 0.0), lin_RGB_mouse.g, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(200.0, 0.0), lin_RGB_mouse.b, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(350.0, 0.0), sat_mouse, 1.0,4.0);
#endif
    
    // draw RGB + saturation at projected gamut clipped position at the second lowest line
#if IS_DISPLAY_TEXT_PROJECTED_RGB_SAT
    float sat_project= C_clipped / L_clipped;
    fragColor+=drawFloat(fragCoord-vec2(000.0, 20.0), projectedRGB.r, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(100.0, 20.0), projectedRGB.g, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(200.0, 20.0), projectedRGB.b, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(350.0, 20.0), sat_project, 1.0,4.0);
#endif
    
    // draw RGB + saturation at cusp at the third lowest line
#if IS_DISPLAY_TEXT_CUSP_RGB_SAT
    vec3  lin_RGB_cusp= oklab_to_rgb(oklab_LCh_to_Lab(cusp.x, cusp.y, hue));
    float sat_cusp     = cusp.y / cusp.x;
    fragColor+=drawFloat(fragCoord-vec2(000.0, 40.0), lin_RGB_cusp.r, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(100.0, 40.0), lin_RGB_cusp.g, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(200.0, 40.0), lin_RGB_cusp.b, 1.0,4.0);
    fragColor+=drawFloat(fragCoord-vec2(350.0, 40.0), sat_cusp, 1.0,4.0);
#endif
    
    // draw gamut cusp
#if IS_DISPLAY_GAMUT_CUSP_POINT
    fragColor+= drawCircleAtLC(fragCoord, cusp, 3.0);
#endif

    // draw mouse position, gamut project to position and gamut clipped position
#if IS_DISPLAY_GAMUT_CLIP_LINE
    fragColor+= drawCircleAtLC(fragCoord, ProjectPtFrom, 3.0);
    fragColor+= drawCircleAtLC(fragCoord, ProjectPtTo  , 3.0);
    fragColor+= drawCircleAtLC(fragCoord, LC_clipped   , 3.0);
    
    // draw project line
    fragColor+= sdLine(fragCoord, CL_to_pxCoord(LC_clipped.yx), CL_to_pxCoord(ProjectPtTo.yx))*0.2;
    fragColor+= sdLine(fragCoord, CL_to_pxCoord(ProjectPtFrom.yx), CL_to_pxCoord(ProjectPtTo.yx));
#endif

}