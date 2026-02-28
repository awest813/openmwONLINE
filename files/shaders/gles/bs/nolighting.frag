#version 300 es
precision highp float;
#pragma import_defines(FORCE_OPAQUE)

#if @diffuseMap
uniform sampler2D diffuseMap;
in vec2 diffuseMapUV;
#endif

in vec3 passViewPos;
in vec3 passNormal;
in float euclideanDepth;
in float linearDepth;
in float passFalloff;

uniform vec2 screenRes;
uniform bool useFalloff;
uniform float far;
uniform float near;
uniform float alphaRef;
uniform mat3 osg_NormalMatrix;

#include "lib/core/fragment.h.glsl"
#include "lib/material/alpha.glsl"

#include "gles/vertexcolors.glsl"
#include "gles/fog.glsl"
#include "gles/shadows_fragment.glsl"

#if @softParticles
#include "lib/particle/soft.glsl"

uniform float particleSize;
uniform bool particleFade;
uniform float softFalloffDepth;
#endif

out vec4 osg_FragColor;

void main()
{
#if @diffuseMap
    osg_FragColor = texture(diffuseMap, diffuseMapUV);
    osg_FragColor.a *= coveragePreservingAlphaScale(diffuseMap, diffuseMapUV);
#else
    osg_FragColor = vec4(1.0);
#endif

    osg_FragColor *= getDiffuseColor();

    if (useFalloff)
        osg_FragColor.a *= passFalloff;

    osg_FragColor.a = alphaTest(osg_FragColor.a, alphaRef);

    osg_FragColor = applyFogAtDist(osg_FragColor, euclideanDepth, linearDepth, far);

#if !defined(FORCE_OPAQUE) && @softParticles
    vec2 screenCoords = gl_FragCoord.xy / screenRes;
    vec3 viewVec = normalize(passViewPos.xyz);
    vec3 viewNormal = normalize(osg_NormalMatrix * passNormal);

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
