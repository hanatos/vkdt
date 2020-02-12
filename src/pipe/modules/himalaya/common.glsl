// Himalayas. Created by Reinder Nijhoff 2018
// @reindernijhoff
//
// https://www.shadertoy.com/view/MdGfzh
//
// This is my first attempt to render volumetric clouds in a fragment shader.
//
// 1 unit correspondents to SCENE_SCALE meter.

#define SCENE_SCALE (10.)
#define INV_SCENE_SCALE (.1)

#define MOUNTAIN_HEIGHT (5000.)
#define MOUNTAIN_HW_RATIO (0.00016)

#define SUN_DIR normalize(vec3(-.7,.5,.75))
#define SUN_COLOR (vec3(1.,.9,.85)*1.4)

#define FLAG_POSITION (vec3(3900.5,720.,-2516.)*INV_SCENE_SCALE)
#define HUMANOID_SCALE (2.)

#define CAMERA_RO (vec3(3980.,730.,-2650.)*INV_SCENE_SCALE)
#define CAMERA_FL 2.

#define HEIGHT_BASED_FOG_B 0.02
#define HEIGHT_BASED_FOG_C 0.05


mat3 getCamera( in float time, in vec4 mouse, inout vec3 ro, inout vec3 ta ) {
    ro = CAMERA_RO;
    vec3 cw;
    if (mouse.z > 0.) {
        vec2 m = (mouse.xy - .5) * 2.3;
        float my = -sin(m.y);
        cw = normalize(vec3(-sin(-m.x), my+.15, cos(-m.x)));
    } else {
    	ro.x += -cos(time*.13)*5.*INV_SCENE_SCALE;
    	ro.z += (-cos(time*.1)*100.+20.)*INV_SCENE_SCALE;
    	cw = normalize(vec3(-.1,.18,1.));
    }   
    ta = ro + cw*(200.*INV_SCENE_SCALE);
	vec3 cp = vec3(0.0,1.0, 0.0);
	vec3 cu = normalize( cross(cw,cp) );
	vec3 cv = normalize( cross(cu,cw) );
    return mat3( cu, cv, cw );
}

void getRay( in float time, in vec2 fragCoord, in vec2 resolution, in vec4 mouse, inout vec3 ro, inout vec3 rd) {
	vec3 ta;
	mat3 cam = getCamera( time, mouse, ro, ta );
    vec2 p = (-resolution.xy + 2.0*(fragCoord))/resolution.y;
    rd = cam * normalize(vec3(p,CAMERA_FL));     
}

//
// To reduce noise I use temporal reprojection (both for clouds (Buffer D) and the terrain 
// (Buffer C) seperatly. The temporal repojection code is based on code from the shader
// "Rain Forest" (again by Íñigo Quílez):
//
// https://www.shadertoy.com/view/4ttSWf
// 
vec4 saveCamera( in float time, in vec2 fragCoord, in vec4 mouse ) {   
    vec3 ro, ta;
    mat3 cam = getCamera( time, mouse, ro, ta );
    vec4 fragColor;
    
    if( abs(fragCoord.x-4.5)<0.5 ) fragColor = vec4( cam[2], -dot(cam[2],ro) );
    if( abs(fragCoord.x-3.5)<0.5 ) fragColor = vec4( cam[1], -dot(cam[1],ro) );
    if( abs(fragCoord.x-2.5)<0.5 ) fragColor = vec4( cam[0], -dot(cam[0],ro) );
    
    return fragColor;
}

vec2 reprojectPos( in vec3 pos, in vec2 resolution, in sampler2D storage ) {
    mat4 oldCam = mat4( texelFetch(storage,ivec2(2,0),0),
                        texelFetch(storage,ivec2(3,0),0),
                        texelFetch(storage,ivec2(4,0),0),
                        0.0, 0.0, 0.0, 1.0 );

    vec4 wpos = vec4(pos,1.0);
    vec3 cpos = (wpos*oldCam).xyz; 
    vec2 npos = CAMERA_FL * cpos.xy / cpos.z;
    return 0.5 + 0.5*npos*vec2(resolution.y/resolution.x,1.0);
}

//
// Fast skycolor function by Íñigo Quílez
// https://www.shadertoy.com/view/MdX3Rr
//
vec3 getSkyColor(vec3 rd) {
    float sundot = clamp(dot(rd,SUN_DIR),0.0,1.0);
	vec3 col = vec3(0.2,0.5,0.85)*1.1 - max(rd.y,0.01)*max(rd.y,0.01)*0.5;
    col = mix( col, 0.85*vec3(0.7,0.75,0.85), pow(1.0-max(rd.y,0.0), 6.0) );

    col += 0.25*vec3(1.0,0.7,0.4)*pow( sundot,5.0 );
    col += 0.25*vec3(1.0,0.8,0.6)*pow( sundot,64.0 );
    col += 0.20*vec3(1.0,0.8,0.6)*pow( sundot,512.0 );
    
    col += clamp((0.1-rd.y)*10., 0., 1.) * vec3(.0,.1,.2);
    col += 0.2*vec3(1.0,0.8,0.6)*pow( sundot, 8.0 );
    return col;
}

bool letterBox(vec2 fragCoord, const vec2 resolution, const float aspect) { 
    if( fragCoord.x < 0. || fragCoord.x > resolution.x ||
        abs(2.*fragCoord.y-resolution.y) > resolution.x * (1./aspect) ) {
        return true;
    } else {
        return false;
    }
}

//
// Noise functions
//
// Hash without Sine by DaveHoskins 
//
// https://www.shadertoy.com/view/4djSRW
//
float hash12( vec2 p ) {
    p  = 50.0*fract( p*0.3183099 );
    return fract( p.x*p.y*(p.x+p.y) );
}

float hash13(vec3 p3) {
    p3  = fract(p3 * 1031.1031);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

vec3 hash33(vec3 p3) {
	p3 = fract(p3 * vec3(.1031, .1030, .0973));
    p3 += dot(p3, p3.yxz+19.19);
    return fract((p3.xxy + p3.yxx)*p3.zyx);
}

float valueHash(vec3 p3) {
    p3  = fract(p3 * 0.1031);
    p3 += dot(p3, p3.yzx + 19.19);
    return fract((p3.x + p3.y) * p3.z);
}

//
// Noise functions used for cloud shapes
//
float valueNoise( in vec3 x, float tile ) {
    vec3 p = floor(x);
    vec3 f = fract(x);
    f = f*f*(3.0-2.0*f);
	
    return mix(mix(mix( valueHash(mod(p+vec3(0,0,0),tile)), 
                        valueHash(mod(p+vec3(1,0,0),tile)),f.x),
                   mix( valueHash(mod(p+vec3(0,1,0),tile)), 
                        valueHash(mod(p+vec3(1,1,0),tile)),f.x),f.y),
               mix(mix( valueHash(mod(p+vec3(0,0,1),tile)), 
                        valueHash(mod(p+vec3(1,0,1),tile)),f.x),
                   mix( valueHash(mod(p+vec3(0,1,1),tile)), 
                        valueHash(mod(p+vec3(1,1,1),tile)),f.x),f.y),f.z);
}

float voronoi( vec3 x, float tile ) {
    vec3 p = floor(x);
    vec3 f = fract(x);

    float res = 100.;
    for(int k=-1; k<=1; k++){
        for(int j=-1; j<=1; j++) {
            for(int i=-1; i<=1; i++) {
                vec3 b = vec3(i, j, k);
                vec3 c = p + b;

                if( tile > 0. ) {
                    c = mod( c, vec3(tile) );
                }

                vec3 r = vec3(b) - f + hash13( c );
                float d = dot(r, r);

                if(d < res) {
                    res = d;
                }
            }
        }
    }

    return 1.-res;
}

float tilableVoronoi( vec3 p, const int octaves, float tile ) {
    float f = 1.;
    float a = 1.;
    float c = 0.;
    float w = 0.;

    if( tile > 0. ) f = tile;

    for( int i=0; i<octaves; i++ ) {
        c += a*voronoi( p * f, f );
        f *= 2.0;
        w += a;
        a *= 0.5;
    }

    return c / w;
}

float tilableFbm( vec3 p, const int octaves, float tile ) {
    float f = 1.;
    float a = 1.;
    float c = 0.;
    float w = 0.;

    if( tile > 0. ) f = tile;

    for( int i=0; i<octaves; i++ ) {
        c += a*valueNoise( p * f, f );
        f *= 2.0;
        w += a;
        a *= 0.5;
    }

    return c / w;
}


