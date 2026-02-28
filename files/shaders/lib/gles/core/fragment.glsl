#version 300 es
precision highp float;

#include "lib/core/fragment.h.glsl"

uniform sampler2D reflectionMap;

vec4 sampleReflectionMap(vec2 uv)
{
    return texture(reflectionMap, uv);
}

#if @waterRefraction
uniform sampler2D refractionMap;
uniform sampler2D refractionDepthMap;

vec4 sampleRefractionMap(vec2 uv)
{
    return texture(refractionMap, uv);
}

float sampleRefractionDepthMap(vec2 uv)
{
    return texture(refractionDepthMap, uv).x;
}

#endif

uniform sampler2D lastShader;

vec4 samplerLastShader(vec2 uv)
{
    return texture(lastShader, uv);
}

#if @skyBlending
uniform sampler2D sky;

vec3 sampleSkyColor(vec2 uv)
{
    return texture(sky, uv).xyz;
}
#endif

uniform sampler2D opaqueDepthTex;

vec4 sampleOpaqueDepthTex(vec2 uv)
{
    return texture(opaqueDepthTex, uv);
}
