// Himalayas. Created by Reinder Nijhoff 2018
// @reindernijhoff
//
// https://www.shadertoy.com/view/MdGfzh
//
// This is my first attempt to render volumetric clouds in a fragment shader.
//
// I started this shader by trying to implement the clouds of Horizon Zero Dawn, as
// described in "The real-time volumetric cloudscapes of Horizon Zero Dawn" by 
// Andrew Schneider and Nathan Vos.[1] To model the shape of the clouds, two look-up
// textures are created with different frequencies of (Perlin -) Worley noise:
//
// Buffer A: The main look-up texture for the cloud shapes. 
// Buffer B: A 3D (32x32x32) look-up texture with Worley Noise used to add small details 
//           to the shapes of the clouds. I have packed this 3D texture into a 2D buffer.
//           
// Because it is not possible (yet) to create buffers with fixed size, or 3D buffers, the
// look-up texture in Buffer A is 2D, and a slice of the volume that is described in the 
// article. Therefore, and because I didn't have any slots left (in Buffer C) to use a 
// cloud type/cloud coverage texture, the modelling of the cloud shapes in this shader is 
// in the end mostly based on trial and error, and is probably far from the code used in 
// Horizon Zero Dawn.
//
// Buffer D: Rendering of the clouds.
//
// I render the clouds using the improved integration method of volumetric media, as described 
// in "Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite" by 
// Sébastien Hillaire.[2]
//
// You can find the (excellent) example shaders of Sébastien Hillaire (SebH) here:
//
// https://www.shadertoy.com/view/XlBSRz
// https://www.shadertoy.com/view/MdlyDs
//
#define CLOUD_MARCH_STEPS 12
#define CLOUD_SELF_SHADOW_STEPS 6

#define EARTH_RADIUS    (1500000.) // (6371000.)
#define CLOUDS_BOTTOM   (1350.)
#define CLOUDS_TOP      (2150.)

#define CLOUDS_LAYER_BOTTOM   (-150.)
#define CLOUDS_LAYER_TOP      (-70.)

#define CLOUDS_COVERAGE (.52)
#define CLOUDS_LAYER_COVERAGE (.41)

#define CLOUDS_DETAIL_STRENGTH (.2)
#define CLOUDS_BASE_EDGE_SOFTNESS (.1)
#define CLOUDS_BOTTOM_SOFTNESS (.25)
#define CLOUDS_DENSITY (.03)
#define CLOUDS_SHADOW_MARGE_STEP_SIZE (10.)
#define CLOUDS_LAYER_SHADOW_MARGE_STEP_SIZE (4.)
#define CLOUDS_SHADOW_MARGE_STEP_MULTIPLY (1.3)
#define CLOUDS_FORWARD_SCATTERING_G (.8)
#define CLOUDS_BACKWARD_SCATTERING_G (-.2)
#define CLOUDS_SCATTERING_LERP (.5)

#define CLOUDS_AMBIENT_COLOR_TOP (vec3(149., 167., 200.)*(1.5/255.))
#define CLOUDS_AMBIENT_COLOR_BOTTOM (vec3(39., 67., 87.)*(1.5/255.))
#define CLOUDS_MIN_TRANSMITTANCE .1

#define CLOUDS_BASE_SCALE 1.51
#define CLOUDS_DETAIL_SCALE 20.

//
// Cloud shape modelling and rendering 
//
float HenyeyGreenstein( float sundotrd, float g) {
	float gg = g * g;
	return (1. - gg) / pow( 1. + gg - 2. * g * sundotrd, 1.5);
}

float interectCloudSphere( vec3 rd, float r ) {
    float b = EARTH_RADIUS * rd.y;
    float d = b * b + r * r + 2. * EARTH_RADIUS * r;
    return -b + sqrt( d );
}

float linearstep( const float s, const float e, float v ) {
    return clamp( (v-s)*(1./(e-s)), 0., 1. );
}

float linearstep0( const float e, float v ) {
    return min( v*(1./e), 1. );
}

float remap(float v, float s, float e) {
	return (v - s) / (e - s);
}

float cloudMapBase(vec3 p, float norY) {
  vec3 uv = p * (0.00005 * CLOUDS_BASE_SCALE);

  vec3 cloud = texture(img_noise0, uv.xz).rgb;

  float n = norY*norY;
  n *= cloud.b ;
  n+= pow(1.-norY, 16.); 
  return remap( cloud.r - n, cloud.g, 1.);
}

float cloudMapDetail(vec3 p) { 
  // 3d lookup in 2d texture :(
  p = abs(p) * (0.0016 * CLOUDS_BASE_SCALE * CLOUDS_DETAIL_SCALE);

  float yi = mod(p.y,32.);
  ivec2 offset = ivec2(mod(yi,8.), mod(floor(yi/8.),4.))*34 + 1;
  float a = texture(img_noise1, (mod(p.xz,32.)+vec2(offset.xy)+1.)/textureSize(img_noise1, 0).xy).r;

  yi = mod(p.y+1.,32.);
  offset = ivec2(mod(yi,8.), mod(floor(yi/8.),4.))*34 + 1;
  float b = texture(img_noise1, (mod(p.xz,32.)+vec2(offset.xy)+1.)/textureSize(img_noise1, 0).xy).r;

  return mix(a,b,fract(p.y));
}

float cloudGradient( float norY ) {
    return linearstep( 0., .05, norY ) - linearstep( .8, 1.2, norY);
}

float cloudMap(vec3 pos, vec3 rd, float norY) {
    vec3 ps = pos;
    
    float m = cloudMapBase(ps, norY);
	m *= cloudGradient( norY );

	float dstrength = smoothstep(1., 0.5, m);
    
    // erode with detail
    if(dstrength > 0.) {
		m -= cloudMapDetail( ps ) * dstrength * CLOUDS_DETAIL_STRENGTH;
    }

	m = smoothstep( 0., CLOUDS_BASE_EDGE_SOFTNESS, m+(CLOUDS_COVERAGE-1.) );
    m *= linearstep0(CLOUDS_BOTTOM_SOFTNESS, norY);

    return clamp(m * CLOUDS_DENSITY * (1.+max((ps.x-7000.)*0.005,0.)), 0., 1.);
}

float volumetricShadow(in vec3 from, in float sundotrd ) {
    float dd = CLOUDS_SHADOW_MARGE_STEP_SIZE;
    vec3 rd = SUN_DIR;
    float d = dd * .5;
    float shadow = 1.0;

    for(int s=0; s<CLOUD_SELF_SHADOW_STEPS; s++) {
        vec3 pos = from + rd * d;
        float norY = (length(pos) - (EARTH_RADIUS + CLOUDS_BOTTOM)) * (1./(CLOUDS_TOP - CLOUDS_BOTTOM));

        if(norY > 1.) return shadow;

        float muE = cloudMap( pos, rd, norY );
        shadow *= exp(-muE * dd);

        dd *= CLOUDS_SHADOW_MARGE_STEP_MULTIPLY;
        d += dd;
    }
    return shadow;
}

vec4 renderClouds( vec3 ro, vec3 rd, inout float dist ) {
    if( rd.y < 0. ) {
        return vec4(0,0,0,10);
    }

    ro.xz *= SCENE_SCALE;
    ro.y = sqrt(EARTH_RADIUS*EARTH_RADIUS-dot(ro.xz,ro.xz));

    float start = interectCloudSphere( rd, CLOUDS_BOTTOM );
    float end  = interectCloudSphere( rd, CLOUDS_TOP );
    
    if (start > dist) {
        return vec4(0,0,0,10);
    }
    
    end = min(end, dist);
    
    float sundotrd = dot( rd, -SUN_DIR);

    // raymarch
    float d = start;
    float dD = (end-start) / float(CLOUD_MARCH_STEPS);

    float h = hash13(rd + fract(0.05 * global.frame) );
    d -= dD * h;

    float scattering =  mix( HenyeyGreenstein(sundotrd, CLOUDS_FORWARD_SCATTERING_G),
        HenyeyGreenstein(sundotrd, CLOUDS_BACKWARD_SCATTERING_G), CLOUDS_SCATTERING_LERP );

    float transmittance = 1.0;
    vec3 scatteredLight = vec3(0.0, 0.0, 0.0);

    dist = EARTH_RADIUS;

    for(int s=0; s<CLOUD_MARCH_STEPS; s++) {
        vec3 p = ro + d * rd;

        float norY = clamp( (length(p) - (EARTH_RADIUS + CLOUDS_BOTTOM)) * (1./(CLOUDS_TOP - CLOUDS_BOTTOM)), 0., 1.);

        float alpha = cloudMap( p, rd, norY );

        if( alpha > 0. ) {
            dist = min( dist, d);
            vec3 ambientLight = mix( CLOUDS_AMBIENT_COLOR_BOTTOM, CLOUDS_AMBIENT_COLOR_TOP, norY );

            vec3 S = (ambientLight + SUN_COLOR * (scattering * volumetricShadow(p, sundotrd))) * alpha;
            float dTrans = exp(-alpha * dD);
            vec3 Sint = (S - S * dTrans) * (1. / alpha);
            scatteredLight += transmittance * Sint; 
            transmittance *= dTrans;
        }

        if( transmittance <= CLOUDS_MIN_TRANSMITTANCE ) break;

        d += dD;
    }

    return vec4(scatteredLight, transmittance);
}

//
//
// !Because I wanted a second cloud layer (below the horizon), I copy-pasted 
// almost all of the code above:
//

float cloudMapLayer(vec3 pos, vec3 rd, float norY) {
    vec3 ps = pos;

    float m = cloudMapBase(ps, norY);
	// m *= cloudGradient( norY );
	float dstrength = smoothstep(1., 0.5, m);
    
    // erode with detail
    if (dstrength > 0.) {
		m -= cloudMapDetail( ps ) * dstrength * CLOUDS_DETAIL_STRENGTH;
    }

	m = smoothstep( 0., CLOUDS_BASE_EDGE_SOFTNESS, m+(CLOUDS_LAYER_COVERAGE-1.) );

    return clamp(m * CLOUDS_DENSITY, 0., 1.);
}

float volumetricShadowLayer(in vec3 from, in float sundotrd ) {
    float dd = CLOUDS_LAYER_SHADOW_MARGE_STEP_SIZE;
    vec3 rd = SUN_DIR;
    float d = dd * .5;
    float shadow = 1.0;

    for(int s=0; s<CLOUD_SELF_SHADOW_STEPS; s++) {
        vec3 pos = from + rd * d;
        float norY = clamp( (pos.y - CLOUDS_LAYER_BOTTOM ) * (1./(CLOUDS_LAYER_TOP - CLOUDS_LAYER_BOTTOM)), 0., 1.);

        if(norY > 1.) return shadow;

        float muE = cloudMapLayer( pos, rd, norY );
        shadow *= exp(-muE * dd);

        dd *= CLOUDS_SHADOW_MARGE_STEP_MULTIPLY;
        d += dd;
    }
    return shadow;
}

vec4 renderCloudLayer( vec3 ro, vec3 rd, inout float dist ) {
    if( rd.y > 0. ) {
        return vec4(0,0,0,10);
    }

    ro.xz *= SCENE_SCALE;
    ro.y = 0.;

    float start = CLOUDS_LAYER_TOP/rd.y;
    float end  = CLOUDS_LAYER_BOTTOM/rd.y;
    
    if (start > dist) {
        return vec4(0,0,0,10);
    }
    
    end = min(end, dist);
    
    float sundotrd = dot( rd, -SUN_DIR);

    // raymarch
    float d = start;
    float dD = (end-start) / float(CLOUD_MARCH_STEPS);

    float h = hash13(rd + fract(0.05 * global.frame) );
    d -= dD * h;

    float scattering =  mix( HenyeyGreenstein(sundotrd, CLOUDS_FORWARD_SCATTERING_G),
        HenyeyGreenstein(sundotrd, CLOUDS_BACKWARD_SCATTERING_G), CLOUDS_SCATTERING_LERP );

    float transmittance = 1.0;
    vec3 scatteredLight = vec3(0.0, 0.0, 0.0);

    dist = EARTH_RADIUS;

    for(int s=0; s<CLOUD_MARCH_STEPS; s++) {
        vec3 p = ro + d * rd;

        float norY = clamp( (p.y - CLOUDS_LAYER_BOTTOM ) * (1./(CLOUDS_LAYER_TOP - CLOUDS_LAYER_BOTTOM)), 0., 1.);

        float alpha = cloudMapLayer( p, rd, norY );

        if( alpha > 0. ) {
            dist = min( dist, d);
            vec3 ambientLight = mix( CLOUDS_AMBIENT_COLOR_BOTTOM, CLOUDS_AMBIENT_COLOR_TOP, norY );

            vec3 S = .7 * (ambientLight +  SUN_COLOR * (scattering * volumetricShadowLayer(p, sundotrd))) * alpha;
            float dTrans = exp(-alpha * dD);
            vec3 Sint = (S - S * dTrans) * (1. / alpha);
            scatteredLight += transmittance * Sint; 
            transmittance *= dTrans;
        }

        if( transmittance <= CLOUDS_MIN_TRANSMITTANCE ) break;

        d += dD;
    }

    return vec4(scatteredLight, transmittance);
}

#if 0
//
// Main function
//
bool resolutionChanged() {
    return floor(texelFetch(iChannel1, ivec2(0), 0).r) != floor(iResolution.x);
}

bool mouseChanged() {
    return iMouse.z * texelFetch(iChannel1, ivec2(1,0), 1).w < 0.;
}

void mainImage( out vec4 fragColor, in vec2 fragCoord ) {            
    if (fragCoord.y < 1.5) {
        fragColor = saveCamera(iTime, fragCoord, iMouse/iResolution.xyxy);
        if( abs(fragCoord.x-1.5)<0.5 ) fragColor = vec4(iMouse);
        if( abs(fragCoord.x-0.5)<0.5 ) fragColor = mouseChanged() ? vec4(0) : vec4(iResolution.xy,0,0);
    } else {
        if( letterBox(fragCoord, iResolution.xy, 2.25) ) {
        	fragColor = vec4( 0., 0., 0., 1. );
       		return;
        } else {
            float dist = texelFetch(iChannel2, ivec2(fragCoord),0).w * SCENE_SCALE;
            vec4 col = vec4(0,0,0,1);
            
            vec3 ro, rd;
    		getRay( iTime, fragCoord, iResolution.xy, iMouse/iResolution.xyxy, ro, rd);

            if( rd.y > 0. ) {
                // clouds
                col = renderClouds(ro, rd, dist);
                float fogAmount = 1.-(.1 + exp(-dist*0.0001));
                col.rgb = mix(col.rgb, getSkyColor(rd)*(1.-col.a), fogAmount);
            } else {
                // cloud layer below horizon
                col = renderCloudLayer(ro, rd, dist);
                // height based fog, see http://iquilezles.org/www/articles/fog/fog.htm
                float fogAmount = HEIGHT_BASED_FOG_C * 
                    (1.-exp( -dist*rd.y*(INV_SCENE_SCALE*HEIGHT_BASED_FOG_B)))/rd.y;
                col.rgb = mix(col.rgb, getSkyColor(rd)*(1.-col.a), clamp(fogAmount,0.,1.));
            }

            if( col.w > 1. ) {
                fragColor = vec4(0,0,0,1);
            } else {
                vec2 spos = reprojectPos(ro+rd*dist, iResolution.xy, iChannel1);
                vec2 rpos = spos * iResolution.xy;

        		if( !letterBox(rpos.xy, iResolution.xy, 2.3) 
                    && !resolutionChanged() && !mouseChanged()) {
                    vec4 ocol = texture( iChannel1, spos, 0.0 ).xyzw;
                    col = mix(ocol, col, 0.05);
                }
                fragColor = col;
            }
        }
    }
}
#endif
