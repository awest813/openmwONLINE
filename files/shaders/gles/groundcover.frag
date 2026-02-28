#version 300 es
precision highp float;

#define GROUNDCOVER

#if @diffuseMap
uniform sampler2D diffuseMap;
in vec2 diffuseMapUV;
#endif

#if @normalMap
uniform sampler2D normalMap;
in vec2 normalMapUV;
#endif

#define PER_PIXEL_LIGHTING @normalMap

in float euclideanDepth;
in float linearDepth;
uniform vec2 screenRes;
uniform float far;
uniform float alphaRef;
uniform float osg_FrontMaterialShininess;

#if PER_PIXEL_LIGHTING
in vec3 passViewPos;
#else
centroid in vec3 passLighting;
centroid in vec3 shadowDiffuseLighting;
#endif

in vec3 passNormal;

#include "gles/shadows_fragment.glsl"
#include "lib/light/lighting.glsl"
#include "lib/material/alpha.glsl"
#include "gles/fog.glsl"
#include "gles/normals.glsl"

out vec4 osg_FragColor;

void main()
{
#if @diffuseMap
    osg_FragColor = texture(diffuseMap, diffuseMapUV);
#else
    osg_FragColor = vec4(1.0);
#endif

    if (euclideanDepth > @groundcoverFadeStart)
        osg_FragColor.a *= 1.0-smoothstep(@groundcoverFadeStart, @groundcoverFadeEnd, euclideanDepth);

    osg_FragColor.a = alphaTest(osg_FragColor.a, alphaRef);

#if @normalMap
    vec4 normalTex = texture(normalMap, normalMapUV);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
#if @reconstructNormalZ
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
#endif
    vec3 viewNormal = normalToView(normal);
#else
    vec3 viewNormal = normalToView(normalize(passNormal));
#endif

    float shadowing = unshadowedLightRatio(linearDepth);

    vec3 lighting;
#if !PER_PIXEL_LIGHTING
    lighting = passLighting + shadowDiffuseLighting * shadowing;
#else
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(passViewPos, viewNormal, osg_FrontMaterialShininess, shadowing, diffuseLight, ambientLight, specularLight);
    lighting = diffuseLight + ambientLight;
#endif

    clampLightingResult(lighting);

    osg_FragColor.xyz *= lighting;
    osg_FragColor = applyFogAtDist(osg_FragColor, euclideanDepth, linearDepth, far);

    applyShadowDebugOverlay();
}
