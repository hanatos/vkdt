#version 440

uniform int width;
uniform int height;
uniform float depositFactor;
uniform int colorModeType;
uniform int numberOfColorModes;

layout(std430,binding=3) buffer mutex
{
	uint particlesCounter[];
};

layout(rg16f,binding=0) uniform readonly image2D trailRead;
layout(rg16f,binding=1) uniform writeonly image2D trailWrite;
layout(rgba8,binding=4) uniform writeonly image2D displayWrite;

/////////////////////////////////////
// color utils, not necessarily used

vec3 rgb2hsv(vec3 c) {
    vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
    vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));

    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}

vec3 changeHue(vec3 hsv, float hueChange) {
    hsv.x += hueChange; // Add the hue change
    hsv.x = mod(hsv.x, 1.0); // Wrap hue to stay within [0, 1]
    return hsv;
}

vec3 hsv2rgb(vec3 c) {
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 pal(in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d) {
    return a + b * cos(6.28318 * (c * t + d));
}
/////////////////////////////////////

// Generic gradient interpolation
vec3 interpolateGradient5(float f, const vec3 cols[5]) {
    f = clamp(f, 0.0, 1.0);
    float cur = f * 4.0; // 5 colors => (5-1) = 4
    int icur = int(floor(cur));
    int next = min(icur + 1, 4);
    return mix(cols[icur], cols[next], fract(cur));
}

vec3 interpolateGradient6(float f, const vec3 cols[6]) {
    f = clamp(f, 0.0, 1.0);
    float cur = f * 5.0;
    int icur = int(floor(cur));
    int next = min(icur + 1, 5);
    return mix(cols[icur], cols[next], fract(cur));
}

vec3 interpolateGradient7(float f, const vec3 cols[7]) {
    f = clamp(f, 0.0, 1.0);
    float cur = f * 6.0;
    int icur = int(floor(cur));
    int next = min(icur + 1, 6);
    return mix(cols[icur], cols[next], fract(cur));
}

const vec3 zorgPurple[5] = vec3[5](vec3(0.0), vec3(0.0), vec3(0.07, 0.18, 0.38), vec3(1., 0., 0.56), vec3(0.58, 1., 0.2));

const vec3 orangeBlue[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.1, 0.2, 0.4), vec3(0.0, 0.5, 0.6), vec3(0.8, 0.4, 0.2), vec3(0.9, 0.6, 0.3), vec3(1.0, 0.9, 0.0));

const vec3 green[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.0, 0.3, 0.2), vec3(0.1, 0.7, 0.7), vec3(0.8, 0.5, 0.3), vec3(0.9, 0.7, 0.5), vec3(1.0, 1.0, 1.0));

const vec3 tealSunset[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.05, 0.2, 0.3), vec3(0.2, 0.4, 0.5), vec3(0.6, 0.3, 0.4), vec3(0.8, 0.4, 0.3), vec3(1.0, 0.5, 0.2));

const vec3 forestNight[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.0, 0.1, 0.0), vec3(0.0, 0.3, 0.2), vec3(0.1, 0.4, 0.4), vec3(0.3, 0.6, 0.6), vec3(0.8, 0.9, 1.0));

const vec3 purpleFire[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.1, 0.3, 0.6), vec3(0.3, 0.2, 0.5), vec3(0.7, 0.2, 0.3), vec3(0.9, 0.5, 0.2), vec3(1.0, 0.9, 0.1));

const vec3 arctic[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.0, 0.1, 0.3), vec3(0.0, 0.3, 0.5), vec3(0.1, 0.6, 0.8), vec3(0.4, 0.8, 1.0), vec3(0.85, 0.96, 1.0));

const vec3 cyan[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.0, 0.3, 0.4), vec3(0.0, 0.5, 0.6), vec3(0.1, 0.7, 0.8), vec3(0.3, 0.9, 0.9), vec3(0.6, 1.0, 1.0));

const vec3 neonInferno[6] = vec3[6](vec3(0.0), vec3(0.2, 0.0, 0.3), vec3(0.6, 0.0, 0.6), vec3(0.8, 0.1, 0.2), vec3(1.0, 0.5, 0.1), vec3(1.0));

const vec3 solarDrift[7] = vec3[7](vec3(0.0), vec3(0.0), vec3(0.3, 0.1, 0.0), vec3(0.6, 0.2, 0.0), vec3(0.9, 0.5, 0.1), vec3(1.0, 0.8, 0.2), vec3(1.0, 0.95, 0.7));

const vec3 plasmaTwilight[5] = vec3[5](vec3(0.0), vec3(0.0, 0.2, 0.5), vec3(0.2, 0.4, 0.9), vec3(0.5, 0.2, 0.7), vec3(1.0, 0.3, 0.7));

// Gradient functions
vec3 gradZorgPurple(float f) {
    return interpolateGradient5(f, zorgPurple);
}
vec3 gradOrangeBlue(float f) {
    return interpolateGradient7(f, orangeBlue);
}
vec3 gradGreen(float f) {
    return interpolateGradient7(f, green);
}
vec3 gradTealSunset(float f) {
    return interpolateGradient7(f, tealSunset);
}
vec3 gradForestNight(float f) {
    return interpolateGradient7(f, forestNight);
}
vec3 gradPurpleFire(float f) {
    return interpolateGradient7(f, purpleFire);
}
vec3 gradArctic(float f) {
    return interpolateGradient7(f, arctic);
}
vec3 gradCyan(float f) {
    return interpolateGradient7(f, cyan);
}
vec3 gradNeonInferno(float f) {
    return interpolateGradient6(f, neonInferno);
}
vec3 gradSolarDrift(float f) {
    return interpolateGradient7(f, solarDrift);
}
vec3 gradPlasmaTwilight(float f) {
    return interpolateGradient5(f, plasmaTwilight);
}

/////////////////////////////////////////////////////
// This shader is looking at a single pixel.
// It adds deposit to the trail map from the number of particles on this pixel.
// It also sets the color of the pixel in the displayed image.

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;
void main() {
    ivec2 pix = ivec2(gl_GlobalInvocationID.xy);

    vec2 prevColor = imageLoad(trailRead, ivec2(gl_GlobalInvocationID.xy)).xy; // Getting the trail map color on current pixel

    float count = float(particlesCounter[gl_GlobalInvocationID.x * height + gl_GlobalInvocationID.y]); // number of particles on the pixel

	// The following 3 lines of code are my own innovation (looks like with the license, attribution is required if you use this :) ),
	// a way to define an amount of added trail in function of the number of particles on the pixel
    const float LIMIT = 100.0;
    float limitedCount = min(count, LIMIT);
    float addedDeposit = sqrt(limitedCount) * depositFactor;

	// Trail map update
    float val = prevColor.x + addedDeposit;
    imageStore(trailWrite, ivec2(gl_GlobalInvocationID.xy), vec4(val, prevColor.y, 0, 0)); // using the second color component to keep track of the previous color, it's not easy to explain why I do this

	// Mapping the count on pixel to color intensity, looks like one day I tried hard to get something satisfying with a complicated formula
    float countColorValue = pow(tanh(7.5 * pow(max(0., (count - 1) / 1000.0), 0.3)), 8.5) * 1.1;
    countColorValue = min(1.0, countColorValue);
	//float colorValue2 = pow(tanh(7.5*pow(max(0.,(250*val-1)/1000.0),0.3)),8.5)*1.05;

	// Mapping the trail map intensity to some color intensity, using trail map color with a delay
    float trailColorValue = pow(tanh(9.0 * pow(max(0., (250 * prevColor.y - 1) / 1100.0), 0.3)), 8.5) * 1.05;
    trailColorValue = min(1.0, trailColorValue);

	// getting a radial offset for color changes depending on pixel position
    // radial offset for color changes depending on pixel position
    vec2 pos = vec2(pix) - vec2(float(width) * 0.5, float(height) * 0.5);
    pos *= (2.0 / float(width + height)) * 0.6;
    float offset = length(pos);

	// something for a color experiment:
	// difference between latest trail map value and delayed trail map value
    float temporalDiff = prevColor.x - prevColor.y;

	// variables to compute:
    vec4 outputColor = vec4(0., 1., 0., 1.); // (dummy overwritten value)
    vec3 col = vec3(0., 1., 0.); // (dummy overwritten value)

    // reused variables
    float blend = tanh(500.0 * temporalDiff + 2.0 * offset);
    vec3 col2 = vec3(countColorValue);

    if(colorModeType == 0) // white on black from particle counts
    {
        col = vec3(countColorValue);
    } else if(colorModeType == 1) // "blueish orange/purple"
    {
        vec3 col1 = gradPurpleFire(tanh(countColorValue * 1.3));
        vec3 col3 = gradArctic(tanh(countColorValue * 1.3));
        col = mix(col1, col3, blend);
        col = clamp(1.25*col,0.,1.);
    } else if(colorModeType == 2) // icy blue
    {
        vec3 col1 = gradArctic(fract(tanh(countColorValue * 0.6 + offset) + 0.15));
        col = mix(col1, col2, blend);
    } else if(colorModeType == 3) // orange over purple, not very saturated
    {
        vec3 col1 = gradPurpleFire(tanh(countColorValue * 1.3));
        col = mix(col1, col2, blend);
    } else if(colorModeType == 4) // gold over dark green
    {
        vec3 col1 = gradOrangeBlue(tanh(countColorValue * 1.3 + offset));
        col = mix(col1, col2, blend);
    } else if(colorModeType == 5) {
        vec3 col1 = gradNeonInferno(tanh(countColorValue * 1.3));
        col = mix(col1, col2, blend);
    } else if(colorModeType == 6) // pink/purple (from z0rg :)
    {
        vec3 col1 = gradZorgPurple(fract(tanh(countColorValue * 0.6 + offset) + 0.15)); // weird, but let's keep it like this
        col = mix(col1, col2, blend);
        vec3 col3 = gradArctic(tanh(countColorValue * 1.3));
        col = mix(col1, col3, blend);
        col = clamp(1.5*pow(col,vec3(1.1)),0.,1.);
    } else if(colorModeType == 7)
    {
        vec3 col1 = gradNeonInferno(tanh(countColorValue * 1.3));
        // col = mix(col1, col2, blend);
        vec3 col3 = gradArctic(tanh(countColorValue * 1.3));
        col = mix(col1, col3, blend);
        col = clamp(1.1*col,0.,1.);
    } else if(colorModeType == 8) // bright is yellow, over blue background, embarassingly experimental
    {
        vec3 col1 = gradOrangeBlue(tanh(countColorValue * 1.3 + offset));
        vec3 colGreen = gradGreen(tanh(countColorValue * 2.3 + offset));
        vec3 col2_ = mix(vec3(clamp(1.3 * countColorValue, 0., 1.)), colGreen, 0.5);
        vec3 col3 = mix(col2_, col1, 1. - 0.6 * tanh(sin(-1500. * abs(temporalDiff)) + 2.0 * offset));
        vec3 col4 = gradOrangeBlue(tanh(countColorValue * 1.3 + offset));
        vec3 col5 = vec3(countColorValue);
        vec3 col6 = 1.25 * mix(col4, col5, tanh(500. * temporalDiff + 2.0 * offset));
        col6 = pow(col6, vec3(2.0));
        col = max(col6, col3);
    } else if(colorModeType == 9) // green
    {
        vec3 col1 = gradGreen(tanh(countColorValue * 1.3));
        col = mix(col1, col2, blend);
        // vec3 col3 = gradArctic(tanh(countColorValue * 1.3));
        // col = mix(col1, col3, blend);
        // col = clamp(1.1*col,0.,1.);
    } else if(colorModeType == 10000) // smooth/blurry cyan with red movement
    {
		// Color defined with combination of particle count (red) and trail map (green, blue)
        col = vec3(countColorValue, trailColorValue, trailColorValue);
    }

    col = clamp(col, vec3(0.), vec3(1.));
    outputColor = vec4(col, 1.0);

    imageStore(displayWrite, ivec2(gl_GlobalInvocationID.xy), outputColor);
}
