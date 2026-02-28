#version 300 es
precision highp float;
#pragma import_defines(FORCE_OPAQUE, DISTORTION)

#define PER_PIXEL_LIGHTING 1

#if @diffuseMap
uniform sampler2D diffuseMap;
in vec2 diffuseMapUV;
#endif

#if @emissiveMap
uniform sampler2D emissiveMap;
in vec2 emissiveMapUV;
#endif

#if @normalMap
uniform sampler2D normalMap;
in vec2 normalMapUV;
#endif

in float euclideanDepth;
in float linearDepth;

in vec3 passViewPos;
in vec3 passNormal;

uniform vec2 screenRes;
uniform float far;
uniform float alphaRef;
uniform float emissiveMult;
uniform float specStrength;
uniform bool useTreeAnim;
uniform float distortionStrength;
uniform mat3 osg_NormalMatrix;
uniform float osg_FrontMaterialShininess;

#include "lib/core/fragment.h.glsl"
#include "lib/light/lighting.glsl"
#include "lib/material/alpha.glsl"
#include "lib/util/distortion.glsl"

#include "gles/vertexcolors.glsl"
#include "gles/shadows_fragment.glsl"
#include "gles/fog.glsl"
#include "gles/normals.glsl"

out vec4 osg_FragColor;

void main()
{
#if @diffuseMap
    osg_FragColor = texture(diffuseMap, diffuseMapUV);

#if defined(DISTORTION) && DISTORTION
    vec2 screenCoords = gl_FragCoord.xy / (screenRes * @distorionRTRatio);
    osg_FragColor.a *= getDiffuseColor().a;
    osg_FragColor = applyDistortion(osg_FragColor, distortionStrength, gl_FragCoord.z, sampleOpaqueDepthTex(screenCoords).x);
    return;
#endif

    osg_FragColor.a *= coveragePreservingAlphaScale(diffuseMap, diffuseMapUV);
#else
    osg_FragColor = vec4(1.0);
#endif

    vec4 diffuseColor = getDiffuseColor();
    if (!useTreeAnim)
        osg_FragColor.a *= diffuseColor.a;
    osg_FragColor.a = alphaTest(osg_FragColor.a, alphaRef);

    vec3 specularColor = getSpecularColor().xyz;
#if @normalMap
    vec4 normalTex = texture(normalMap, normalMapUV);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
#if @reconstructNormalZ
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
#endif
    vec3 viewNormal = normalToView(normal);
    specularColor *= normalTex.a;
#else
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

    float shadowing = unshadowedLightRatio(linearDepth);
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(passViewPos, viewNormal, osg_FrontMaterialShininess, shadowing, diffuseLight, ambientLight, specularLight);
    vec3 diffuse = diffuseColor.xyz * diffuseLight;
    vec3 ambient = getAmbientColor().xyz * ambientLight;
    vec3 emission = getEmissionColor().xyz * emissiveMult;
#if @emissiveMap
    emission *= texture(emissiveMap, emissiveMapUV).xyz;
#endif
    vec3 lighting = diffuse + ambient + emission;
    vec3 specular = specularColor * specularLight * specStrength;

    clampLightingResult(lighting);

    osg_FragColor.xyz = osg_FragColor.xyz * lighting + specular;

    osg_FragColor = applyFogAtDist(osg_FragColor, euclideanDepth, linearDepth, far);

#if defined(FORCE_OPAQUE) && FORCE_OPAQUE
    osg_FragColor.a = 1.0;
#endif

    applyShadowDebugOverlay();
}
