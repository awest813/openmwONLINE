#version 300 es
precision highp float;
#pragma import_defines(FORCE_OPAQUE, DISTORTION)

#if @diffuseMap
uniform sampler2D diffuseMap;
in vec2 diffuseMapUV;
#endif

#if @darkMap
uniform sampler2D darkMap;
in vec2 darkMapUV;
#endif

#if @detailMap
uniform sampler2D detailMap;
in vec2 detailMapUV;
#endif

#if @decalMap
uniform sampler2D decalMap;
in vec2 decalMapUV;
#endif

#if @emissiveMap
uniform sampler2D emissiveMap;
in vec2 emissiveMapUV;
#endif

#if @normalMap
uniform sampler2D normalMap;
in vec2 normalMapUV;
#endif

#if @envMap
uniform sampler2D envMap;
in vec2 envMapUV;
uniform vec4 envMapColor;
#endif

#if @specularMap
uniform sampler2D specularMap;
in vec2 specularMapUV;
#endif

#if @bumpMap
uniform sampler2D bumpMap;
in vec2 bumpMapUV;
uniform vec2 envMapLumaBias;
uniform mat2 bumpMapMatrix;
#endif

#if @glossMap
uniform sampler2D glossMap;
in vec2 glossMapUV;
#endif

uniform vec2 screenRes;
uniform float near;
uniform float far;
uniform float alphaRef;
uniform float distortionStrength;
uniform mat3 osg_NormalMatrix;
uniform float osg_FrontMaterialShininess;

#define PER_PIXEL_LIGHTING (@normalMap || @specularMap || @forcePPL)

#if !PER_PIXEL_LIGHTING
centroid in vec3 passLighting;
centroid in vec3 passSpecular;
centroid in vec3 shadowDiffuseLighting;
centroid in vec3 shadowSpecularLighting;
#else
uniform float emissiveMult;
uniform float specStrength;
#endif
in vec3 passViewPos;
in vec3 passNormal;
#if @normalMap || @diffuseParallax
in vec4 passTangent;
#endif

#if @additiveBlending
#define ADDITIVE_BLENDING
#endif

#include "lib/core/fragment.h.glsl"
#include "lib/light/lighting.glsl"
#include "lib/material/parallax.glsl"
#include "lib/material/alpha.glsl"
#include "lib/util/distortion.glsl"

#include "gles/fog.glsl"
#include "gles/vertexcolors.glsl"
#include "gles/shadows_fragment.glsl"
#include "gles/normals.glsl"

#if @softParticles
#include "lib/particle/soft.glsl"

uniform float particleSize;
uniform bool particleFade;
uniform float softFalloffDepth;
#endif

#if @particleOcclusion
#include "lib/particle/occlusion.glsl"
uniform sampler2D orthoDepthMap;
in vec3 orthoDepthMapCoord;
#endif

out vec4 osg_FragColor;

void main()
{
#if @particleOcclusion
    applyOcclusionDiscard(orthoDepthMapCoord, texture(orthoDepthMap, orthoDepthMapCoord.xy * 0.5 + 0.5).r);
#endif

    vec2 offset = vec2(0.0);

#if @parallax || @diffuseParallax
#if @parallax
    float height = texture(normalMap, normalMapUV).a;
#else
    float height = texture(diffuseMap, diffuseMapUV).a;
#endif
    offset = getParallaxOffset(transpose(normalToViewMatrix) * normalize(-passViewPos), height);
#endif

vec2 screenCoords = gl_FragCoord.xy / screenRes;

#if @diffuseMap
    osg_FragColor = texture(diffuseMap, diffuseMapUV + offset);

#if defined(DISTORTION) && DISTORTION
    osg_FragColor.a *= getDiffuseColor().a;
    osg_FragColor = applyDistortion(osg_FragColor, distortionStrength, gl_FragCoord.z, sampleOpaqueDepthTex(screenCoords / @distorionRTRatio).x);
    return;
#endif

#if @diffuseParallax
    osg_FragColor.a = 1.0;
#else
    osg_FragColor.a *= coveragePreservingAlphaScale(diffuseMap, diffuseMapUV + offset);
#endif
#else
    osg_FragColor = vec4(1.0);
#endif

    vec4 diffuseColor = getDiffuseColor();
    osg_FragColor.a *= diffuseColor.a;

#if @darkMap
    osg_FragColor *= texture(darkMap, darkMapUV);
    osg_FragColor.a *= coveragePreservingAlphaScale(darkMap, darkMapUV);
#endif

    osg_FragColor.a = alphaTest(osg_FragColor.a, alphaRef);

#if @normalMap
    vec4 normalTex = texture(normalMap, normalMapUV + offset);
    vec3 normal = normalTex.xyz * 2.0 - 1.0;
#if @reconstructNormalZ
    normal.z = sqrt(1.0 - dot(normal.xy, normal.xy));
#endif
    vec3 viewNormal = normalToView(normal);
#else
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);
#endif

    vec3 viewVec = normalize(passViewPos);

#if @detailMap
    osg_FragColor.xyz *= texture(detailMap, detailMapUV).xyz * 2.0;
#endif

#if @decalMap
    vec4 decalTex = texture(decalMap, decalMapUV);
    osg_FragColor.xyz = mix(osg_FragColor.xyz, decalTex.xyz, decalTex.a * diffuseColor.a);
#endif

#if @envMap

    vec2 envTexCoordGen = envMapUV;
    float envLuma = 1.0;

#if @normalMap
    vec3 r = reflect( viewVec, viewNormal );
    float m = 2.0 * sqrt( r.x*r.x + r.y*r.y + (r.z+1.0)*(r.z+1.0) );
    envTexCoordGen = vec2(r.x/m + 0.5, r.y/m + 0.5);
#endif

#if @bumpMap
    vec4 bumpTex = texture(bumpMap, bumpMapUV);
    envTexCoordGen += bumpTex.rg * bumpMapMatrix;
    envLuma = clamp(bumpTex.b * envMapLumaBias.x + envMapLumaBias.y, 0.0, 1.0);
#endif

    vec3 envEffect = texture(envMap, envTexCoordGen).xyz * envMapColor.xyz * envLuma;

#if @glossMap
    envEffect *= texture(glossMap, glossMapUV).xyz;
#endif

#if @preLightEnv
    osg_FragColor.xyz += envEffect;
#endif

#endif

    float shadowing = unshadowedLightRatio(-passViewPos.z);
    vec3 lighting, specular;
#if !PER_PIXEL_LIGHTING
    lighting = passLighting + shadowDiffuseLighting * shadowing;
    specular = passSpecular + shadowSpecularLighting * shadowing;
#else
#if @specularMap
    vec4 specTex = texture(specularMap, specularMapUV);
    float shininess = specTex.a * 255.0;
    vec3 specularColor = specTex.xyz;
#else
    float shininess = osg_FrontMaterialShininess;
    vec3 specularColor = getSpecularColor().xyz;
#endif
    vec3 diffuseLight, ambientLight, specularLight;
    doLighting(passViewPos, viewNormal, shininess, shadowing, diffuseLight, ambientLight, specularLight);
    lighting = diffuseColor.xyz * diffuseLight + getAmbientColor().xyz * ambientLight + getEmissionColor().xyz * emissiveMult;
    specular = specularColor * specularLight * specStrength;
#endif

    clampLightingResult(lighting);
    osg_FragColor.xyz = osg_FragColor.xyz * lighting + specular;

#if @envMap && !@preLightEnv
    osg_FragColor.xyz += envEffect;
#endif

#if @emissiveMap
    osg_FragColor.xyz += texture(emissiveMap, emissiveMapUV).xyz;
#endif

    osg_FragColor = applyFogAtPos(osg_FragColor, passViewPos, far);

#if !defined(FORCE_OPAQUE) && @softParticles
    osg_FragColor.a *= calcSoftParticleFade(
        viewVec,
        passViewPos,
        viewNormal,
        near,
        far,
        sampleOpaqueDepthTex(screenCoords).x,
        particleSize,
        particleFade,
        softFalloffDepth
    );
#endif

#if defined(FORCE_OPAQUE) && FORCE_OPAQUE
    osg_FragColor.a = 1.0;
#endif

    applyShadowDebugOverlay();
}
