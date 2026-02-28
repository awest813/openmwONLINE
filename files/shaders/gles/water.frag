#version 300 es
precision highp float;

#include "lib/core/fragment.h.glsl"

const float VISIBILITY = 2500.0;
const float VISIBILITY_DEPTH = VISIBILITY * 1.5;
const float DEPTH_FADE = 0.15;

const vec2 BIG_WAVES = vec2(0.1, 0.1);
const vec2 MID_WAVES = vec2(0.1, 0.1);
const vec2 MID_WAVES_RAIN = vec2(0.2, 0.2);
const vec2 SMALL_WAVES = vec2(0.1, 0.1);
const vec2 SMALL_WAVES_RAIN = vec2(0.3, 0.3);

const float WAVE_CHOPPYNESS = 0.05;
const float WAVE_SCALE = 75.0;

const float BUMP = 0.5;
const float BUMP_RAIN = 2.5;
const float REFL_BUMP = 0.10;
const float REFR_BUMP = 0.07;

#if @sunlightScattering
const float SCATTER_AMOUNT = 0.3;
const vec3 SCATTER_COLOUR = vec3(0.0,1.0,0.95);
const vec3 SUN_EXT = vec3(0.45, 0.55, 0.68);
#endif

const float SUN_SPEC_FADING_THRESHOLD = 0.15;
const float SPEC_HARDNESS = 256.0;
const float SPEC_BUMPINESS = 5.0;
const float SPEC_BRIGHTNESS = 1.5;

const float BUMP_SUPPRESS_DEPTH = 300.0;
const float REFR_FOG_DISTORT_DISTANCE = 3000.0;

const vec2 WIND_DIR = vec2(0.5, -0.8);
const float WIND_SPEED = 0.2;

const vec3 WATER_COLOR = vec3(0.090195, 0.115685, 0.12745);

#if @wobblyShores
const float WOBBLY_SHORE_FADE_DISTANCE = 6200.0;
#endif

vec2 normalCoords(vec2 uv, float scale, float speed, float time, float timer1, float timer2, vec3 previousNormal)
{
  return uv * (WAVE_SCALE * scale) + WIND_DIR * time * (WIND_SPEED * speed) -(previousNormal.xy/previousNormal.zz) * WAVE_CHOPPYNESS + vec2(time * timer1,time * timer2);
}

uniform sampler2D rippleMap;
uniform vec3 playerPos;

in vec3 worldPos;
in vec2 rippleMapUV;
in vec4 position;
in float linearDepth;

uniform sampler2D normalMap;

uniform float osg_SimulationTime;

uniform float near;
uniform float far;

uniform float rainIntensity;

uniform vec2 screenRes;

uniform mat4 osg_ModelViewMatrixInverse;
uniform mat3 osg_NormalMatrix;
uniform vec4 osg_LightModelAmbient;

#define PER_PIXEL_LIGHTING 0

#include "gles/shadows_fragment.glsl"
#include "lib/light/lighting.glsl"
#include "gles/fog.glsl"
#include "lib/water/fresnel.glsl"
#include "lib/water/rain_ripples.glsl"
#include "lib/view/depth.glsl"

out vec4 osg_FragColor;

void main(void)
{
    vec2 UV = worldPos.xy / (8192.0*5.0) * 3.0;

    float shadow = unshadowedLightRatio(linearDepth);

    vec2 screenCoords = gl_FragCoord.xy / screenRes;

    #define waterTimer osg_SimulationTime

    vec3 normal0 = 2.0 * texture(normalMap,normalCoords(UV, 0.05, 0.04, waterTimer, -0.015, -0.005, vec3(0.0,0.0,0.0))).rgb - 1.0;
    vec3 normal1 = 2.0 * texture(normalMap,normalCoords(UV, 0.1,  0.08, waterTimer,  0.02,   0.015, normal0)).rgb - 1.0;
    vec3 normal2 = 2.0 * texture(normalMap,normalCoords(UV, 0.25, 0.07, waterTimer, -0.04,  -0.03,  normal1)).rgb - 1.0;
    vec3 normal3 = 2.0 * texture(normalMap,normalCoords(UV, 0.5,  0.09, waterTimer,  0.03,   0.04,  normal2)).rgb - 1.0;
    vec3 normal4 = 2.0 * texture(normalMap,normalCoords(UV, 1.0,  0.4,  waterTimer, -0.02,   0.1,   normal3)).rgb - 1.0;
    vec3 normal5 = 2.0 * texture(normalMap,normalCoords(UV, 2.0,  0.7,  waterTimer,  0.1,   -0.06,  normal4)).rgb - 1.0;

    vec4 rainRipple;

    if (rainIntensity > 0.01)
        rainRipple = rainCombined(position.xy/1000.0, waterTimer) * clamp(rainIntensity, 0.0, 1.0);
    else
        rainRipple = vec4(0.0);

    vec3 rippleAdd = rainRipple.xyz * 10.0;

    float distToCenter = length(rippleMapUV - vec2(0.5));
    float blendClose = smoothstep(0.001, 0.02, distToCenter);
    float blendFar = 1.0 - smoothstep(0.3, 0.4, distToCenter);
    float distortionLevel = 2.0;
    rippleAdd += distortionLevel * vec3(texture(rippleMap, rippleMapUV).ba * blendFar * blendClose, 0.0);

    vec2 bigWaves = BIG_WAVES;
    vec2 midWaves = mix(MID_WAVES, MID_WAVES_RAIN, rainIntensity);
    vec2 smallWaves = mix(SMALL_WAVES, SMALL_WAVES_RAIN, rainIntensity);
    float bump = mix(BUMP,BUMP_RAIN,rainIntensity);

    vec3 normal = (normal0 * bigWaves.x + normal1 * bigWaves.y + normal2 * midWaves.x +
                   normal3 * midWaves.y + normal4 * smallWaves.x + normal5 * smallWaves.y + rippleAdd);
    normal = normalize(vec3(-normal.x * bump, -normal.y * bump, normal.z));

    vec3 sunWorldDir = normalize((osg_ModelViewMatrixInverse * vec4(lcalcPosition(0).xyz, 0.0)).xyz);
    vec3 cameraPos = (osg_ModelViewMatrixInverse * vec4(0,0,0,1)).xyz;
    vec3 viewDir = normalize(position.xyz - cameraPos.xyz);

    float sunFade = length(osg_LightModelAmbient.xyz);

    float ior = (cameraPos.z>0.0)?(1.333/1.0):(1.0/1.333);
    float fresnel = clamp(fresnel_dielectric(viewDir, normal, ior), 0.0, 1.0);

    vec2 screenCoordsOffset = normal.xy * REFL_BUMP;
#if @waterRefraction
    float depthSample = linearizeDepth(sampleRefractionDepthMap(screenCoords), near, far);
    float surfaceDepth = linearizeDepth(gl_FragCoord.z, near, far);
    float realWaterDepth = depthSample - surfaceDepth;
    float depthSampleDistorted = linearizeDepth(sampleRefractionDepthMap(screenCoords - screenCoordsOffset), near, far);
    float waterDepthDistorted = max(depthSampleDistorted - surfaceDepth, 0.0);
    screenCoordsOffset *= clamp(realWaterDepth / BUMP_SUPPRESS_DEPTH, 0.0, 1.0);
#endif
    vec3 reflection = sampleReflectionMap(screenCoords + screenCoordsOffset).rgb;

    vec3 waterColor = WATER_COLOR * sunFade;

    vec4 sunSpec = lcalcSpecular(0);
    sunSpec.a = min(1.0, sunSpec.a / SUN_SPEC_FADING_THRESHOLD);

    const float SPEC_MAGIC = 1.55;

    vec3 specNormal = normalize(vec3(normal.x * SPEC_BUMPINESS, normal.y * SPEC_BUMPINESS, normal.z));
    vec3 viewReflectDir = reflect(viewDir, specNormal);
    float phongTerm = max(dot(viewReflectDir, sunWorldDir), 0.0);
    float specular = pow(atan(phongTerm * SPEC_MAGIC), SPEC_HARDNESS) * SPEC_BRIGHTNESS;
    specular = clamp(specular, 0.0, 1.0) * shadow * sunSpec.a;

    vec3 skyColorEstimate = vec3(max(0.0, mix(-0.3, 1.0, sunFade)));
    vec3 rainSpecular = abs(rainRipple.w)*mix(skyColorEstimate, vec3(1.0), 0.05)*0.5;
    float waterTransparency = clamp(fresnel * 6.0 + specular, 0.0, 1.0);

#if @waterRefraction
    if (cameraPos.z > 0.0 && realWaterDepth <= VISIBILITY_DEPTH && waterDepthDistorted > VISIBILITY_DEPTH)
        screenCoordsOffset = vec2(0.0);

    depthSampleDistorted = linearizeDepth(sampleRefractionDepthMap(screenCoords - screenCoordsOffset), near, far);
    waterDepthDistorted = max(depthSampleDistorted - surfaceDepth, 0.0);

    waterDepthDistorted = mix(waterDepthDistorted, realWaterDepth, min(surfaceDepth / REFR_FOG_DISTORT_DISTANCE, 1.0));

    vec3 refraction = sampleRefractionMap(screenCoords - screenCoordsOffset).rgb;
    vec3 rawRefraction = refraction;

    if (cameraPos.z < 0.0)
        refraction = clamp(refraction * 1.5, 0.0, 1.0);
    else
    {
        float depthCorrection = sqrt(1.0 + 4.0 * DEPTH_FADE * DEPTH_FADE);
        float factor = DEPTH_FADE * DEPTH_FADE / (-0.5 * depthCorrection + 0.5 - waterDepthDistorted / VISIBILITY) + 0.5 * depthCorrection + 0.5;
        refraction = mix(refraction, waterColor, clamp(factor, 0.0, 1.0));
    }

#if @sunlightScattering
    vec3 scatterNormal = (normal0 * bigWaves.x * 0.5 + normal1 * bigWaves.y * 0.5 + normal2 * midWaves.x * 0.2 +
                          normal3 * midWaves.y * 0.2 + normal4 * smallWaves.x * 0.1 + normal5 * smallWaves.y * 0.1 + rippleAdd);
    scatterNormal = normalize(vec3(-scatterNormal.xy * bump, scatterNormal.z));
    float sunHeight = sunWorldDir.z;
    vec3 scatterColour = mix(SCATTER_COLOUR * vec3(1.0, 0.4, 0.0), SCATTER_COLOUR, max(1.0 - exp(-sunHeight * SUN_EXT), 0.0));
    float scatterLambert = max(dot(sunWorldDir, scatterNormal) * 0.7 + 0.3, 0.0);
    float scatterReflectAngle = max(dot(reflect(sunWorldDir, scatterNormal), viewDir) * 2.0 - 1.2, 0.0);
    float lightScatter = scatterLambert * scatterReflectAngle * SCATTER_AMOUNT * sunFade * sunSpec.a * max(1.0 - exp(-sunHeight), 0.0);
    refraction = mix(refraction, scatterColour, lightScatter);
#endif

    osg_FragColor.rgb = mix(refraction, reflection, fresnel);
    osg_FragColor.a = 1.0;
    rainSpecular *= waterTransparency;
#else
    osg_FragColor.rgb = mix(waterColor, reflection, (1.0 + fresnel) * 0.5);
    osg_FragColor.a = waterTransparency;
#endif

    osg_FragColor.rgb += specular * sunSpec.rgb + rainSpecular;

#if @waterRefraction && @wobblyShores
    vec3 normalShoreRippleRain = texture(normalMap,normalCoords(UV, 2.0, 2.7, -1.0*waterTimer,  0.05,  0.1,  normal3)).rgb - 0.5
                               + texture(normalMap,normalCoords(UV, 2.0, 2.7,      waterTimer,  0.04, -0.13, normal4)).rgb - 0.5;
    float viewFactor = mix(abs(viewDir.z), 1.0, 0.2);
    float verticalWaterDepth = realWaterDepth * viewFactor;
    float shoreOffset = verticalWaterDepth - (normal2.r + mix(0.0, normalShoreRippleRain.r, rainIntensity) + 0.15)*8.0;
    float fuzzFactor = min(1.0, 1000.0 / surfaceDepth) * viewFactor;
    shoreOffset *= fuzzFactor;
    shoreOffset = clamp(mix(shoreOffset, 1.0, clamp(linearDepth / WOBBLY_SHORE_FADE_DISTANCE, 0.0, 1.0)), 0.0, 1.0);
    osg_FragColor.rgb = mix(rawRefraction, osg_FragColor.rgb, shoreOffset);
#endif

#if @radialFog
    float radialDepth = distance(position.xyz, cameraPos);
#else
    float radialDepth = 0.0;
#endif

    osg_FragColor = applyFogAtDist(osg_FragColor, radialDepth, linearDepth, far);

#if !@disableNormals
    // Second render target not supported in single-output GLES
#endif

    applyShadowDebugOverlay();
}
