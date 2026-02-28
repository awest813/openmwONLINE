#if @skyBlending
#include "lib/core/fragment.h.glsl"

uniform float skyBlendingStart;
#endif

uniform float osg_FogStart;
uniform float osg_FogEnd;
uniform float osg_FogScale;
uniform vec4 osg_FogColor;

vec4 applyFogAtDist(vec4 color, float euclideanDist, float linearDist, float far)
{
#if @radialFog
    float dist = euclideanDist;
#else
    float dist = abs(linearDist);
#endif
#if @exponentialFog
    float fogValue = 1.0 - exp(-2.0 * max(0.0, dist - osg_FogStart / 2.0) / (osg_FogEnd - osg_FogStart / 2.0));
#else
    float fogValue = clamp((dist - osg_FogStart) * osg_FogScale, 0.0, 1.0);
#endif
#ifdef ADDITIVE_BLENDING
    color.xyz *= 1.0 - fogValue;
#else
    color.xyz = mix(color.xyz, osg_FogColor.xyz, fogValue);
#endif

#if @skyBlending
    float fadeValue = clamp((far - dist) / (far - skyBlendingStart), 0.0, 1.0);
    fadeValue *= fadeValue;
#ifdef ADDITIVE_BLENDING
    color.xyz *= fadeValue;
#else
    color.xyz = mix(sampleSkyColor(gl_FragCoord.xy / screenRes), color.xyz, fadeValue);
#endif
#endif

    return color;
}

vec4 applyFogAtPos(vec4 color, vec3 pos, float far)
{
    return applyFogAtDist(color, length(pos), pos.z, far);
}
