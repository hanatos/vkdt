#version 440

// By Etienne Jacob, License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License
// Attribution to Sage Jenson's work explained in comments

// warning: these must be compatible with the c++ code
#define MAX_NUMBER_OF_WAVES 5
#define MAX_NUMBER_OF_RANDOM_SPAWN 7

#define PI 3.141592

uniform int width;
uniform int height;

uniform float time;

uniform float actionAreaSizeSigma;

uniform float actionX;
uniform float actionY;

uniform float moveBiasActionX;
uniform float moveBiasActionY;

uniform float waveXarray[MAX_NUMBER_OF_WAVES];
uniform float waveYarray[MAX_NUMBER_OF_WAVES];
uniform float waveTriggerTimes[MAX_NUMBER_OF_WAVES];
uniform float waveSavedSigmas[MAX_NUMBER_OF_WAVES];

uniform float mouseXchange;
uniform float L2Action;

uniform int spawnParticles;
uniform float spawnFraction;
uniform int randomSpawnNumber;
uniform float randomSpawnXarray[MAX_NUMBER_OF_RANDOM_SPAWN];
uniform float randomSpawnYarray[MAX_NUMBER_OF_RANDOM_SPAWN];

uniform float pixelScaleFactor;

struct PointSettings {
    int typeIndex;

    float defaultScalingFactor;

    float SensorDistance0;
    float SD_exponent;
    float SD_amplitude;

    float SensorAngle0;
    float SA_exponent;
    float SA_amplitude;

    float RotationAngle0;
    float RA_exponent;
    float RA_amplitude;

    float MoveDistance0;
    float MD_exponent;
    float MD_amplitude;

    float SensorBias1;
    float SensorBias2;
};

layout(std430,binding=5) buffer parameters
{
	PointSettings params[];
};

layout(rg16f,binding=0) uniform readonly image2D trailRead;
layout(std430,binding=3) buffer mutex
{
	uint particlesCounter[];
};

layout(std430, binding=2) buffer particle{
    uint particlesArray[];
};

///////////////////////////////////////////////////
// Randomness utils obtained from chatgpt

float random3(vec3 st) {
    return fract(sin(dot(st.xyz, vec3(12.9898, 78.233, 151.7182))) * 43758.5453123);
}

float noise(vec3 st) {
    vec3 i = floor(st);
    vec3 F = fract(st);

    // Calculate the eight corners of the cube
    float a = random3(i);
    float b = random3(i + vec3(1.0, 0.0, 0.0));
    float c = random3(i + vec3(0.0, 1.0, 0.0));
    float d = random3(i + vec3(1.0, 1.0, 0.0));
    float e = random3(i + vec3(0.0, 0.0, 1.0));
    float f = random3(i + vec3(1.0, 0.0, 1.0));
    float g = random3(i + vec3(0.0, 1.0, 1.0));
    float h = random3(i + vec3(1.0, 1.0, 1.0));

    // Smoothly interpolate the noise value
    vec3 u = F * F * (3.0 - 2.0 * F);

    return mix(mix(mix(a, b, u.x), mix(c, d, u.x), u.y), mix(mix(e, f, u.x), mix(g, h, u.x), u.y), u.z);
}

// A small, stand-alone 32-bit hashing function.
// Maps a single uint 'v' into another "scrambled" uint.
uint pcg_hash(uint v) {
    // These constants come from PCG-like mixing functions
    v = v * 747796405u + 2891336453u;
    uint word = ((v >> ((v >> 28u) + 4u)) ^ v) * 277803737u;
    return (word >> 22u) ^ word;
}
float randFloat(inout uint state) {
    // Hash and update 'state' each time we want a new random
    state = pcg_hash(state);
    // Convert to float in [0..1)
    //  4294967296 = 2^32
    return float(state) * (1.0 / 4294967296.0);
}
vec2 randomPosFromParticle(in vec2 particlePos) {
    // Convert (x,y) to integer coordinates
    ivec2 ipos = ivec2(floor(particlePos));

    // Pack x in the low 16 bits, y in the high 16 bits
    // (Works if width, height <= 65535)
    uint seed = (uint(ipos.x) & 0xFFFFu) | ((uint(ipos.y) & 0xFFFFu) << 16);

    // Generate two random floats in [0..1)
    float rx = randFloat(seed);
    float ry = randFloat(seed);

    // Scale them to [0..width] and [0..height] respectively
    return vec2(rx * width, ry * height);
}
float random01FromParticle(in vec2 particlePos) {
    ivec2 ipos = ivec2(floor(particlePos));
    uint seed = (uint(ipos.x) & 0xFFFFu) | ((uint(ipos.y) & 0xFFFFu) << 16);
    return randFloat(seed);
}
// End of randomness utils
///////////////////////////////////////////////////

float getGridValue(vec2 pos) {
    return imageLoad(trailRead, ivec2(mod(pos.x + 0.5 + float(width), float(width)), mod(pos.y + 0.5 + float(height), float(height)))).x;
}

float senseFromAngle(float angle, vec2 pos, float heading, float so) {
    return getGridValue(vec2(pos.x + so * cos(heading + angle), pos.y + so * sin(heading + angle)));
}

float propagatedWaveFunction(float x, float sigma) {
    //float waveSigma = 0.5*(0.4 + 0.8*waveActionAreaSizeSigma);
    float waveSigma = 0.15 + 0.4 * sigma;
    return float(x <= 0.) * exp(-x * x / waveSigma / waveSigma);
}

// This is the main shader.
// It updates the current particle's attributes (mostly position and heading).
// It also increases a counter on the pixel of the particle's new position, which will be used to add deposit in the deposit shader.
// Counter increased with atomicAdd function to be able to do it with many particles in parallel.

layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
void main() {

    PointSettings currentParams_1 = params[1]; // "Background Point" parameters
    PointSettings currentParams_2 = params[0]; // "Pen Point" parameters

    vec2 particlePos = unpackUnorm2x16(particlesArray[3 * gl_GlobalInvocationID.x]) * vec2(width, height);
    vec2 currAHeading = unpackUnorm2x16(particlesArray[3 * gl_GlobalInvocationID.x + 1]) * vec2(1.0, 2.0 * PI);
    vec2 velocity = unpackHalf2x16(particlesArray[3 * gl_GlobalInvocationID.x + 2]);

    float heading = currAHeading.y;
    vec2 direction = vec2(cos(heading), sin(heading));

    vec2 normalizedPosition = vec2(particlePos.x / width, particlePos.y / height);
    vec2 normalizedActionPosition = vec2(actionX / width, actionY / height);

    // some inputs for noise values used later (not important stuff)
    vec2 positionForNoise1 = normalizedPosition;
    positionForNoise1.x *= float(width) / height;
    vec2 positionForNoise2 = positionForNoise1;
    float noiseScale = 20.0;
    positionForNoise1 *= noiseScale;
    float noiseScale2 = 6.0;
    positionForNoise2 *= noiseScale2;

    // looking for "lerper" variable, the parameter for interpolation between the parameters of the 2 Points (Pen/Background)
    vec2 positionFromAction = normalizedPosition - normalizedActionPosition;
    positionFromAction.x *= float(width) / height;
    float distanceNoiseFactor = (0.9 + 0.2 * noise(vec3(positionForNoise2.x, positionForNoise2.t, 0.6 * time))); // a bit of noise distortion on distance
    float distanceFromAction = distance(positionFromAction, vec2(0)) * distanceNoiseFactor;
    float lerper = exp(-distanceFromAction * distanceFromAction / actionAreaSizeSigma / actionAreaSizeSigma); // use of a gaussian function: 1 at the center, 0 far from the center
    //lerper = diffDist<=actionAreaSizeSigma ? 1 : 0;
    //lerper = particlePos.x/width;

    // Section about the "trigerred waves" interaction, really a secondary feature
    float waveSum = 0.;
    float noiseVariationFactor = (0.95 + 0.1 * noise(vec3(positionForNoise1.x, positionForNoise1.y, 0.3 * time)));
    float maxWaveTime = 5.0; // in seconds

    for(int i = 0; i < MAX_NUMBER_OF_WAVES; i++) {
        if((time - waveTriggerTimes[i]) <= maxWaveTime) {
            vec2 normalizedWaveCenterPosition = vec2(waveXarray[i] / width, waveYarray[i] / height);
            vec2 relDiffWave = normalizedPosition - normalizedWaveCenterPosition;
            relDiffWave.x *= float(width) / height;
            float diffDistWave = distance(relDiffWave, vec2(0));
            float angleToCenter = atan(relDiffWave.y, relDiffWave.x);
            float dir = (i % 2 == 0) ? 1. : -1.;

            float delay = -0.1 + diffDistWave / 0.3 * noiseVariationFactor + 0.4 * pow(0.5 + 0.5 * cos(18. * angleToCenter + 10.0 * dir * diffDistWave), 0.3);
            float varWave = delay - (time - waveTriggerTimes[i]);
            float sigmaVariation = pow(waveSavedSigmas[i], 0.75);
            waveSum += 0.6 * propagatedWaveFunction(varWave, waveSavedSigmas[i]) * max(0., 1. - 0.3 * diffDistWave / sigmaVariation * noiseVariationFactor);
        }
    }

    waveSum = 1.7 * tanh(waveSum / 1.7) + 0.4 * tanh(4. * waveSum);
    //lerper = mix(lerper,0.,tanh(5.*waveSum));

    // Sensing a value at particle pos or next to it...

    // A factor on sensed value, lerp between "pen" and "background" parameters
    float tunedSensorScaler_1 = currentParams_1.defaultScalingFactor;
    float tunedSensorScaler_2 = currentParams_2.defaultScalingFactor;
    float tunedSensorScaler_mix = mix(tunedSensorScaler_1, tunedSensorScaler_2, lerper);
    tunedSensorScaler_mix *= 1.0 + 0.3 * waveSum; // increased sense value on waves

    float SensorBias1_mix = mix(currentParams_1.SensorBias1, currentParams_2.SensorBias1, lerper);
    float SensorBias2_mix = mix(currentParams_1.SensorBias2, currentParams_2.SensorBias2, lerper);

    ///////////////////////////////////////////////////////////////////////////////////
    // Technique/formulas from Sage Jenson (mxsage)
    float currentSensedValue = getGridValue(particlePos + SensorBias2_mix * direction + vec2(0., SensorBias1_mix)) * tunedSensorScaler_mix;
    currentSensedValue = clamp(currentSensedValue, 0.000000001, 1.0);
    ///////////////////////////////////////////////////////////////////////////////////

    // lerp between "pen" and "background" parameters
    float SensorDistance0_mix = mix(currentParams_1.SensorDistance0, currentParams_2.SensorDistance0, lerper);
    float SD_amplitude_mix = mix(currentParams_1.SD_amplitude, currentParams_2.SD_amplitude, lerper);
    float SD_exponent_mix = mix(currentParams_1.SD_exponent, currentParams_2.SD_exponent, lerper);

    float MoveDistance0_mix = mix(currentParams_1.MoveDistance0, currentParams_2.MoveDistance0, lerper);
    float MD_amplitude_mix = mix(currentParams_1.MD_amplitude, currentParams_2.MD_amplitude, lerper);
    float MD_exponent_mix = mix(currentParams_1.MD_exponent, currentParams_2.MD_exponent, lerper);

    float SensorAngle0_mix = mix(currentParams_1.SensorAngle0, currentParams_2.SensorAngle0, lerper);
    float SA_amplitude_mix = mix(currentParams_1.SA_amplitude, currentParams_2.SA_amplitude, lerper);
    float SA_exponent_mix = mix(currentParams_1.SA_exponent, currentParams_2.SA_exponent, lerper);

    float RotationAngle0_mix = mix(currentParams_1.RotationAngle0, currentParams_2.RotationAngle0, lerper);
    float RA_amplitude_mix = mix(currentParams_1.RA_amplitude, currentParams_2.RA_amplitude, lerper);
    float RA_exponent_mix = mix(currentParams_1.RA_exponent, currentParams_2.RA_exponent, lerper);

    ///////////////////////////////////////////////////////////////////////////////////
    // Technique/formulas from Sage Jenson (mxsage)
    // For a current sensed value S,
    // physarum param = A + B * (S ^ C)
    // These A,B,C parameters are part of the data of a "Point"
    float sensorDistance = SensorDistance0_mix + SD_amplitude_mix * pow(currentSensedValue, SD_exponent_mix) * pixelScaleFactor;
    float moveDistance = MoveDistance0_mix + MD_amplitude_mix * pow(currentSensedValue, MD_exponent_mix) * pixelScaleFactor;
    float sensorAngle = SensorAngle0_mix + SA_amplitude_mix * pow(currentSensedValue, SA_exponent_mix);
    float rotationAngle = RotationAngle0_mix + RA_amplitude_mix * pow(currentSensedValue, RA_exponent_mix);
    // 3 * 4 = 12 parameters
    ///////////////////////////////////////////////////////////////////////////////////

    // sensing at 3 positions, as in the classic physarum algorithm
    float sensedLeft = senseFromAngle(-sensorAngle, particlePos, heading, sensorDistance);
    float sensedMiddle = senseFromAngle(0, particlePos, heading, sensorDistance);
    float sensedRight = senseFromAngle(sensorAngle, particlePos, heading, sensorDistance);

    float newHeading = heading;
    // heading update, as in the classic physarum algorithm
    if(sensedMiddle > sensedLeft && sensedMiddle > sensedRight) {
        ;
    } else if(sensedMiddle < sensedLeft && sensedMiddle < sensedRight) {
        newHeading = (random01FromParticle(particlePos) < 0.5 ? heading - rotationAngle : heading + rotationAngle);
    } else if(sensedRight < sensedLeft) {
        newHeading = heading - rotationAngle;
    } else if(sensedLeft < sensedRight) {
        newHeading = heading + rotationAngle;
    }

    // Forcing movement with joystick action,
    // using noise to have more or less of this forced movement, because it looked too boring without
    float noiseValue = noise(vec3(positionForNoise1.x, positionForNoise1.y, 0.8 * time));
    float moveBiasFactor = 5 * lerper * noiseValue;
    vec2 moveBias = moveBiasFactor * vec2(moveBiasActionX, moveBiasActionY);

    // position update of the classic physarum algorithm, but with a new move bias for fun interaction
    float classicNewPositionX = particlePos.x + moveDistance * cos(newHeading) + moveBias.x;
    float classicNewPositionY = particlePos.y + moveDistance * sin(newHeading) + moveBias.y;

    // inertia experimental stuff... actually it's a lot weirder than just modifying speed instead of position
    // probably the weirdest stuff in the code of this project
    velocity *= 0.98;
    float vf = 1.0;
    float velocityBias = 0.2 * L2Action;
    float vx = velocity.x + vf * cos(newHeading) + velocityBias * moveBias.x;
    float vy = velocity.y + vf * sin(newHeading) + velocityBias * moveBias.y;

    //float dt = 0.05*moveDistance;
    float dt = 0.07 * pow(moveDistance, 1.4); // really weird thing, I thought this looked satisfying

    float inertiaNewPositionX = particlePos.x + dt * vx + moveBias.x;
    float inertiaNewPositionY = particlePos.y + dt * vy + moveBias.y;

    float moveStyleLerper = 0.6 * L2Action + 0.8 * waveSum; // intensity of use of inertia
    // the new position of the particle:
    float px = mix(classicNewPositionX, inertiaNewPositionX, moveStyleLerper);
    float py = mix(classicNewPositionY, inertiaNewPositionY, moveStyleLerper);

    // possibility of spawn to other position if spawning action is triggered
    if(spawnParticles >= 1) {
        float randForChoice = random01FromParticle(particlePos * 1.1); // uniform random in [0,1]

        if(randForChoice < spawnFraction) // probability spawnFraction to spawn
        {
            float randForRadius = random01FromParticle(particlePos * 2.2);

            if(spawnParticles == 1) // circular spawn
            {
                float randForTheta = random01FromParticle(particlePos * 3.3);
                float theta = randForTheta * PI * 2.0;
                float r1 = actionAreaSizeSigma * 0.55 * (0.95 + 0.1 * randForRadius);
                float sx = r1 * cos(theta);
                float sy = r1 * sin(theta);
                vec2 spos = vec2(sx, sy);
                spos *= height;
                px = actionX + spos.x;
                py = actionY + spos.y;
            }
            if(spawnParticles == 2) // spawn at few places near pen
            {
                int randForSpawnIndex = int(floor(randomSpawnNumber * random01FromParticle(particlePos * 4.4)));
                float sx = randomSpawnXarray[randForSpawnIndex];
                float sy = randomSpawnYarray[randForSpawnIndex];
                vec2 spos = 0.65 * actionAreaSizeSigma * vec2(sx, sy) * (0.9 + 0.1 * randForRadius);
                spos *= height;
                px = actionX + spos.x;
                py = actionY + spos.y;
            }
        }
    }

    // just position loop to keep pixel positions of the simulation canvas
    vec2 nextPos = vec2(mod(px + float(width), float(width)), mod(py + float(height), float(height)));

    uint depositAmount = uint(1); // all particles add 1 on pixel count, could be more complex one day maybe
    // atomicAdd for increasing counter at pixel, in parallel computation
    atomicAdd(particlesCounter[int(round(nextPos.x)) * height + int(round(nextPos.y))], depositAmount);

    ///////////////////////////////////////////////////////////////////////////////////
    // Technique/formula from Sage Jenson (mxsage)
    // particles are regularly respawning, their progression is stored in particle data
    const float reinitSegment = 0.0010; // respawn every 1/reinitSegment iterations
    float curA = currAHeading.x;
    if(curA < reinitSegment) {
        nextPos = randomPosFromParticle(particlePos);
    }
    float nextA = fract(curA + reinitSegment);
    ///////////////////////////////////////////////////////////////////////////////////

    vec2 nextPosUV = mod(nextPos, vec2(width, height)) / vec2(width, height);
    float newHeadingNorm = mod(newHeading, 2.0 * PI) / (2.0 * PI);
    vec2 nextAandHeading = vec2(nextA, fract(newHeadingNorm));

    // update particle data
    particlesArray[3 * gl_GlobalInvocationID.x] = packUnorm2x16(nextPosUV);
    particlesArray[3 * gl_GlobalInvocationID.x + 1] = packUnorm2x16(nextAandHeading);
    particlesArray[3 * gl_GlobalInvocationID.x + 2] = packHalf2x16(vec2(vx, vy));
}
